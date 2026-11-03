#include "transport/tv_client.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cJSON.h"
#include "core/config.hpp"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "mqtt_client.h"

namespace transport {
namespace {

constexpr char kTag[] = "tv";

// Telemetria Glasshouse waży około 5 kB i rośnie z każdą wersją. Zapas na
// wyrost, bo alokujemy z PSRAM i raz.
constexpr int kAssemblyCapacity = 16384;
// Tyle czekamy po utracie brokera, zanim ekran TV zgaśnie. Przy słabym Wi-Fi
// klient gubi połączenie co kilka minut i wraca po około 3 s z retained stanem;
// bez zwłoki każda taka czkawka pokazywała pasek braku stanu.
constexpr int64_t kOfflineGraceUs = 5 * 1000 * 1000;

// Tematy stanu, które Glasshouse publikuje natychmiast po zmianie (retained).
// Telemetria idzie co 10 s, więc bez nich wybór źródła czekałby na potwierdzenie
// dłużej niż limit oczekiwania.
constexpr char kLeafStatus[] = "status";
constexpr char kLeafTelemetry[] = "telemetry";
constexpr char kLeafApp[] = "state/application/app";
constexpr char kLeafPicture[] = "state/picture/pictureMode";
constexpr char kLeafSystemOn[] = "state/power/systemOn";

// Live TV nie występuje w mapie wejść telemetrii, a jest źródłem jak każde inne.
constexpr char kLiveTvId[] = "livetv";
constexpr char kLiveTvName[] = "TV";

// Klawisz w słowniku polecenia `rcu` Glasshouse.
const char* key_name(domain::NavKey key) {
    switch (key) {
        case domain::NavKey::Up: return "up";
        case domain::NavKey::Down: return "down";
        case domain::NavKey::Left: return "left";
        case domain::NavKey::Right: return "right";
        case domain::NavKey::Ok: return "ok";
        case domain::NavKey::Back: return "back";
        case domain::NavKey::Home: return "home";
        // Przycisk ustawień na pilocie LG: szybkie ustawienia.
        case domain::NavKey::Menu: return "settings";
        default: return nullptr;
    }
}

const char* string_from(const cJSON* parent, const char* key) {
    const cJSON* node = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsString(node) && node->valuestring != nullptr ? node->valuestring : "";
}

bool same_options(const domain::OptionList& a, const domain::OptionList& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

bool same(const domain::TvState& a, const domain::TvState& b) {
    return a.available == b.available && a.power == b.power && a.label == b.label &&
           a.source == b.source && a.picture_mode == b.picture_mode &&
           same_options(a.source_options, b.source_options) &&
           same_options(a.source_ids, b.source_ids) &&
           same_options(a.picture_mode_options, b.picture_mode_options) &&
           same_options(a.picture_mode_ids, b.picture_mode_ids);
}

bool topic_is(const char* topic, int topic_len, const core::Text<48>& prefix, const char* leaf) {
    const std::size_t prefix_len = std::strlen(prefix.c_str());
    const std::size_t leaf_len = std::strlen(leaf);
    return static_cast<std::size_t>(topic_len) == prefix_len + leaf_len &&
           std::strncmp(topic, prefix.c_str(), prefix_len) == 0 &&
           std::strncmp(topic + prefix_len, leaf, leaf_len) == 0;
}

void* alloc_buffer(int size) {
    void* buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    return buffer != nullptr ? buffer : malloc(size);
}

}  // namespace

bool TvClient::start(MqttBus& bus, const storage::MqttSettings& settings) {
    stop();
    if (!settings.tv()) {
        ESP_LOGI(kTag, "brak prefiksu Glasshouse, telewizor pozostaje niedostepny");
        return false;
    }

    settings_ = settings;
    status_.clear();
    app_.clear();
    picture_raw_.clear();
    live_app_ = false;
    live_picture_ = false;
    system_on_ = -1;
    input_count_ = 0;
    mode_count_ = 0;
    last_ = domain::TvState{};
    emitted_ = false;

    if (events_ == nullptr) {
        events_ = xQueueCreate(4, sizeof(TvEvent));
    }
    if (offline_timer_ == nullptr) {
        esp_timer_create_args_t timer = {};
        timer.callback = &TvClient::on_offline_timer;
        timer.arg = this;
        timer.name = "tv_offline";
        esp_timer_handle_t handle = nullptr;
        if (esp_timer_create(&timer, &handle) == ESP_OK) {
            offline_timer_ = handle;
        }
    }
    if (assembly_ == nullptr) {
        assembly_ = static_cast<char*>(alloc_buffer(kAssemblyCapacity));
    }

    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s/", settings_.tv_prefix.c_str());
    prefix_.assign(buffer);
    bus_ = &bus;
    handle_ = bus.handle();
    if (handle_ == nullptr) {
        return false;
    }
    bus.attach(&TvClient::on_mqtt_event, this);
    return true;
}

void TvClient::stop() {
    handle_ = nullptr;
    connected_ = false;
    if (offline_timer_ != nullptr) {
        esp_timer_stop(static_cast<esp_timer_handle_t>(offline_timer_));
    }
    if (events_ != nullptr) {
        xQueueReset(events_);
    }
}

void TvClient::on_offline_timer(void* self_raw) {
    auto* self = static_cast<TvClient*>(self_raw);
    // Połączenie nie wróciło w czasie zwłoki: stan TV przestaje być świeży.
    if (!self->connected_) {
        TvEvent offline;
        offline.kind = TvEvent::Kind::Offline;
        self->emit(offline);
    }
}

bool TvClient::poll(TvEvent& out) {
    return events_ != nullptr && xQueueReceive(events_, &out, 0) == pdTRUE;
}

void TvClient::emit(const TvEvent& event) {
    if (events_ != nullptr) {
        xQueueSend(events_, &event, pdMS_TO_TICKS(100));
    }
}

void TvClient::subscribe_all() {
    auto client = static_cast<esp_mqtt_client_handle_t>(handle_);
    const char* leaves[] = {kLeafStatus, kLeafTelemetry, kLeafApp, kLeafPicture, kLeafSystemOn};
    char topic[96];
    for (const char* leaf : leaves) {
        std::snprintf(topic, sizeof(topic), "%s%s", prefix_.c_str(), leaf);
        esp_mqtt_client_subscribe(client, topic, 1);
    }
}

void TvClient::request_sync() {
    // Glasshouse publikuje stan jako retained, więc ponowna subskrypcja jest
    // żądaniem odświeżenia — tak samo jak przy Z906 (ADR 0006).
    if (handle_ == nullptr || !connected_) {
        return;
    }
    resync_ = true;
    subscribe_all();
}

void TvClient::publish_command(const char* name, const char* value, int qos) {
    if (handle_ == nullptr || !connected_) {
        return;
    }
    char topic[96];
    std::snprintf(topic, sizeof(topic), "%scommand/%s", prefix_.c_str(), name);
    // Wysyła zadanie wysyłki wspólnego połączenia: nie czekamy na brokera w zadaniu UI.
    bus_->publish(topic, value, qos, false);
}

void TvClient::publish_intent(const domain::UiIntent& intent) {
    using domain::ControlId;

    switch (intent.control) {
        case ControlId::TvSource:
            // Polecenie przyjmuje identyfikator wejścia, nie nazwę z ekranu.
            if (!intent.payload.empty()) {
                publish_command("input", intent.payload.c_str(), 1);
            }
            return;
        case ControlId::TvPicture:
            if (!intent.payload.empty()) {
                publish_command("picture_mode", intent.payload.c_str(), 1);
            }
            return;
        case ControlId::TvChannelStep:
            // Klawisze P+/P− pilota LG (KEY_CHANNELUP/DOWN, 402/403). Glasshouse
            // 0.138.0 ich nie zna — patrz docs/integrations/tv.md. Chwilowe jak nawigacja.
            publish_command("rcu", intent.flag ? "channelup" : "channeldown", 0);
            return;
        case ControlId::TvPower:
            // Włączenie z czuwania wymaga Wake-on-LAN, który na TV jest wyłączony.
            // Przycisk zostaje na razie bez działania (ADR 0008).
            return;
        default:
            break;
    }

    if (intent.kind == domain::IntentKind::Navigate && intent.target == "tv") {
        if (const char* key = key_name(intent.key)) {
            // Chwilowe zdarzenie: QoS 0, bez ponowień, jak gładzik Apple TV.
            publish_command("rcu", key, 0);
        }
    }
}

void TvClient::apply_telemetry(const char* data, int length) {
    cJSON* root = cJSON_ParseWithLength(data, static_cast<size_t>(length));
    if (root == nullptr) {
        ESP_LOGW(kTag, "telemetria nie jest poprawnym JSON-em");
        return;
    }

    // Wejścia: Live TV na początku, potem HDMI w kolejności numerów. Nazwy
    // pochodzą z ustawień telewizora („PC”, „Apple TV”), więc nie zaszywamy ich.
    input_count_ = 0;
    inputs_[input_count_].id.assign(kLiveTvId);
    inputs_[input_count_].name.assign(kLiveTvName);
    ++input_count_;
    const cJSON* inputs = cJSON_GetObjectItemCaseSensitive(root, "inputs");
    const std::size_t first_hdmi = input_count_;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, inputs) {
        if (input_count_ >= inputs_.size() || !cJSON_IsString(item) || item->string == nullptr) {
            continue;
        }
        inputs_[input_count_].id.assign(item->string);
        inputs_[input_count_].name.assign(item->valuestring);
        ++input_count_;
    }
    std::sort(inputs_.begin() + first_hdmi, inputs_.begin() + input_count_,
              [](const Option& a, const Option& b) {
                  return std::strcmp(a.id.c_str(), b.id.c_str()) < 0;
              });

    // Tryby obrazu zależą od materiału (SDR, HDR, Dolby Vision): bierzemy te,
    // które telewizor właśnie przyjmie.
    if (const cJSON* picture = cJSON_GetObjectItemCaseSensitive(root, "picture");
        cJSON_IsObject(picture)) {
        const cJSON* modes = cJSON_GetObjectItemCaseSensitive(picture, "modes");
        if (cJSON_IsArray(modes) && cJSON_GetArraySize(modes) > 0) {
            mode_count_ = 0;
            cJSON_ArrayForEach(item, modes) {
                const char* value = string_from(item, "value");
                if (mode_count_ >= modes_.size() || value[0] == '\0') {
                    continue;
                }
                const char* label = string_from(item, "label");
                modes_[mode_count_].id.assign(value);
                modes_[mode_count_].name.assign(label[0] != '\0' ? label : value);
                ++mode_count_;
            }
        }
        // Temat natychmiastowy jest świeższy od telemetrii zebranej przed chwilą.
        if (!live_picture_) {
            picture_raw_.assign(string_from(picture, "mode_raw"));
        }
    }

    if (!live_app_) {
        // `input_source` niesie wybrane przed chwilą wejście, zanim TV je zgłosi.
        const char* app = string_from(root, "input_source");
        app_.assign(app[0] != '\0' ? app : string_from(root, "app"));
    }
    if (system_on_ < 0) {
        if (const cJSON* power = cJSON_GetObjectItemCaseSensitive(root, "powerState");
            cJSON_IsObject(power)) {
            const cJSON* on = cJSON_GetObjectItemCaseSensitive(power, "systemOn");
            if (cJSON_IsBool(on)) {
                system_on_ = cJSON_IsTrue(on) ? 1 : 0;
            }
        }
    }

    cJSON_Delete(root);
}

void TvClient::handle_message(const char* topic, int topic_len, const char* data, int data_len) {
    char value[48];
    const int copy = std::min(data_len, static_cast<int>(sizeof(value)) - 1);
    std::memcpy(value, data, copy);
    value[copy] = '\0';

    if (topic_is(topic, topic_len, prefix_, kLeafTelemetry)) {
        apply_telemetry(data, data_len);
    } else if (topic_is(topic, topic_len, prefix_, kLeafStatus)) {
        status_.assign(value);
    } else if (topic_is(topic, topic_len, prefix_, kLeafApp)) {
        app_.assign(value);
        live_app_ = true;
    } else if (topic_is(topic, topic_len, prefix_, kLeafPicture)) {
        picture_raw_.assign(value);
        live_picture_ = true;
    } else if (topic_is(topic, topic_len, prefix_, kLeafSystemOn)) {
        system_on_ = std::strcmp(value, "true") == 0 ? 1 : 0;
    } else {
        return;
    }
    rebuild();
}

void TvClient::rebuild() {
    TvEvent event;
    event.kind = TvEvent::Kind::Snapshot;
    domain::TvState& tv = event.tv;

    // „off” to telewizor w czuwaniu z działającym serwerem: polecenia jeszcze
    // dochodzą. „asleep” i „offline” znaczą, że nie ma do kogo mówić.
    const bool online = status_ == "online";
    tv.available = online || status_ == "off";
    if (tv.available) {
        tv.power = online && system_on_ != 0 ? domain::Power::On : domain::Power::Off;
    }
    tv.label.assign("LG OLED");

    for (std::size_t i = 0; i < input_count_; ++i) {
        tv.source_options.add(inputs_[i].name.c_str());
        tv.source_ids.add(inputs_[i].id.c_str());
        // Aplikacja spoza wejść (Netflix, ustawienia) nie zaznacza żadnego źródła.
        if (inputs_[i].id == app_) {
            tv.source = inputs_[i].name;
        }
    }
    for (std::size_t i = 0; i < mode_count_; ++i) {
        tv.picture_mode_options.add(modes_[i].name.c_str());
        tv.picture_mode_ids.add(modes_[i].id.c_str());
        if (modes_[i].id == picture_raw_) {
            tv.picture_mode = modes_[i].name;
        }
    }

    // Telemetria przychodzi co 10 s, zwykle bez zmian. Każdy snapshot zamyka
    // oczekiwania na całym pilocie, więc wysyłamy tylko prawdziwą zmianę.
    const bool forced = resync_.exchange(false);
    if (emitted_ && !forced && same(tv, last_)) {
        return;
    }
    last_ = tv;
    emitted_ = true;
    ESP_LOGI(kTag, "stan: %s, zrodlo '%s', obraz '%s'", tv.available ? status_.c_str() : "niedostepny",
             tv.source.c_str(), tv.picture_mode.c_str());
    emit(event);
}

void TvClient::on_mqtt_event(void* self_raw, const char*, int32_t id, void* data) {
    auto* self = static_cast<TvClient*>(self_raw);
    auto* event = static_cast<esp_mqtt_event_handle_t>(data);
    if (self->handle_ == nullptr) {
        return;  // odpięty: połączenie jeszcze gaśnie
    }

    switch (static_cast<esp_mqtt_event_id_t>(id)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(kTag, "polaczono z brokerem");
            self->connected_ = true;
            if (self->offline_timer_ != nullptr) {
                esp_timer_stop(static_cast<esp_timer_handle_t>(self->offline_timer_));
            }
            self->resync_ = true;
            self->subscribe_all();
            break;
        case MQTT_EVENT_DISCONNECTED:
        case MQTT_EVENT_ERROR: {
            if (self->connected_) {
                ESP_LOGW(kTag, "utracono polaczenie z brokerem");
            }
            // Status zostaje: po powrocie broker odeśle retained, a wymuszony
            // obraz (resync_) i tak wyjdzie. Bez połączenia polecenia przepadają,
            // więc oczekiwanie na ekranie skończy się brakiem potwierdzenia.
            self->connected_ = false;
            if (self->offline_timer_ != nullptr &&
                !esp_timer_is_active(static_cast<esp_timer_handle_t>(self->offline_timer_))) {
                esp_timer_start_once(static_cast<esp_timer_handle_t>(self->offline_timer_),
                                     kOfflineGraceUs);
            }
            break;
        }
        case MQTT_EVENT_DATA: {
            // Krótkie tematy stanu mieszczą się w jednej ramce; telemetria nie.
            if (event->current_data_offset == 0 && event->data_len == event->total_data_len) {
                self->handle_message(event->topic, event->topic_len, event->data, event->data_len);
                break;
            }
            if (self->assembly_ == nullptr) {
                break;
            }
            if (event->current_data_offset == 0) {
                self->assembly_len_ = 0;
                self->assembly_total_ = event->total_data_len;
                // Wspólne połączenie: długie wiadomości HA nie są nasze.
                const std::size_t prefix_len = std::strlen(self->prefix_.c_str());
                self->assembly_mine_ =
                    event->topic_len >= static_cast<int>(prefix_len) &&
                    std::strncmp(event->topic, self->prefix_.c_str(), prefix_len) == 0;
                char topic[96];
                const int length = std::min(event->topic_len, static_cast<int>(sizeof(topic)) - 1);
                std::memcpy(topic, event->topic, length);
                topic[length] = '\0';
                self->assembly_topic_.assign(topic);
            }
            if (!self->assembly_mine_) {
                break;
            }
            if (self->assembly_len_ + event->data_len <= kAssemblyCapacity) {
                std::memcpy(self->assembly_ + self->assembly_len_, event->data, event->data_len);
                self->assembly_len_ += event->data_len;
            }
            if (self->assembly_total_ > 0 && self->assembly_len_ >= self->assembly_total_) {
                const char* topic = self->assembly_topic_.c_str();
                self->handle_message(topic, static_cast<int>(std::strlen(topic)), self->assembly_,
                                     self->assembly_len_);
                self->assembly_len_ = 0;
                self->assembly_total_ = 0;
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace transport
