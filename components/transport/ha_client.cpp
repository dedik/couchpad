#include "transport/ha_client.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cJSON.h"
#include "core/clock.hpp"
#include "core/config.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/idf_additions.h"
#include "mqtt_client.h"

namespace transport {
namespace {

constexpr char kTag[] = "ha";

// Wersja w temacie jest częścią kontraktu: zmiana payloadu wymaga nowej.
constexpr char kTopicPrefix[] = "multimedia_remote/v1";

// Snapshot pełnego stanu mieści się w kilkuset bajtach; zapas na wyrost,
// bo alokujemy z PSRAM i raz.
constexpr int kAssemblyCapacity = 8192;

domain::Power power_from(const cJSON* node) {
    if (!cJSON_IsString(node) || node->valuestring == nullptr) {
        return domain::Power::Unknown;
    }
    if (std::strcmp(node->valuestring, "on") == 0) {
        return domain::Power::On;
    }
    if (std::strcmp(node->valuestring, "off") == 0) {
        return domain::Power::Off;
    }
    return domain::Power::Unknown;
}

const char* string_from(const cJSON* parent, const char* key) {
    const cJSON* node = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsString(node) && node->valuestring != nullptr ? node->valuestring : "";
}

// Temperatura z JSON-a (stopnie) na dziesiąte; `fallback`, gdy pola nie ma.
int16_t tenths_from(const cJSON* parent, const char* key, int16_t fallback) {
    const cJSON* node = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsNumber(node)) {
        return fallback;
    }
    return static_cast<int16_t>(std::lround(node->valuedouble * 10.0));
}

void options_from(const cJSON* parent, const char* key, domain::OptionList& out) {
    out.clear();
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(parent, key)) {
        if (cJSON_IsString(item) && item->valuestring != nullptr && item->valuestring[0] != '\0') {
            out.add(item->valuestring);
        }
    }
}

// Sekcja `climate` (ADR 0009). Brak sekcji zostawia klimę niedostępną.
void climate_from(const cJSON* node, domain::ClimateState& climate) {
    // Bez `ClimateState{}`: tymczasowa kopia leżałaby na stosie zadania MQTT.
    // Poniżej i tak przypisujemy każde pole.
    if (!cJSON_IsObject(node)) {
        climate.available = false;
        climate.hvac_mode.clear();
        return;
    }
    climate.available = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(node, "available")) != 0;
    climate.hvac_mode.assign(string_from(node, "hvac_mode"));
    options_from(node, "hvac_modes", climate.hvac_modes);
    climate.target_tenths = tenths_from(node, "target", 0);
    const cJSON* current = cJSON_GetObjectItemCaseSensitive(node, "current");
    climate.has_current = cJSON_IsNumber(current);
    climate.current_tenths = tenths_from(node, "current", 0);
    climate.min_tenths = tenths_from(node, "min", climate.min_tenths);
    climate.max_tenths = tenths_from(node, "max", climate.max_tenths);
    climate.step_tenths = tenths_from(node, "step", climate.step_tenths);
    if (climate.step_tenths <= 0) {
        climate.step_tenths = 5;
    }
    climate.fan_mode.assign(string_from(node, "fan_mode"));
    options_from(node, "fan_modes", climate.fan_modes);
    climate.swing_mode.assign(string_from(node, "swing_mode"));
    options_from(node, "swing_modes", climate.swing_modes);
    climate.swing_horizontal_mode.assign(string_from(node, "swing_horizontal_mode"));
    options_from(node, "swing_horizontal_modes", climate.swing_horizontal_modes);
}

uint16_t u16_from(const cJSON* parent, const char* key) {
    const cJSON* node = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsNumber(node) || node->valuedouble <= 0) {
        return 0;
    }
    return node->valuedouble > 65535 ? 65535 : static_cast<uint16_t>(std::lround(node->valuedouble));
}

void light_from(const cJSON* node, domain::LightState& light) {
    light.id.assign(string_from(node, "id"));
    light.label.assign(string_from(node, "label"));
    light.available = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(node, "available")) != 0;
    light.on = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(node, "on")) != 0;
    const uint16_t brightness = u16_from(node, "brightness");
    light.brightness = static_cast<uint8_t>(brightness > 100 ? 100 : brightness);
    light.kelvin = u16_from(node, "kelvin");
}

// Sekcja `lights` (ADR 0014). Brak sekcji (bridge sprzed tej decyzji) zostawia
// światła niedostępne. Pola przypisujemy po kolei — bez kopii przez stos MQTT.
void lights_from(const cJSON* node, domain::LightsState& lights) {
    lights.count = 0;
    lights.effect_count = 0;
    lights.available = cJSON_IsArray(node) != 0;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, node) {
        if (lights.count >= lights.rooms.size() || !cJSON_IsObject(item)) {
            continue;
        }
        const char* id = string_from(item, "id");
        if (id[0] == '\0') {
            continue;
        }
        domain::RoomLight& room = lights.rooms[lights.count++];
        room.room.assign(string_from(item, "room"));
        light_from(item, room.main);
        room.dimmable = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "dimmable")) != 0;
        room.min_kelvin = u16_from(item, "min_kelvin");
        room.max_kelvin = u16_from(item, "max_kelvin");
        room.effect.assign(string_from(item, "effect"));
        room.member_count = 0;
        const cJSON* member = nullptr;
        cJSON_ArrayForEach(member, cJSON_GetObjectItemCaseSensitive(item, "members")) {
            if (room.member_count >= room.members.size() || !cJSON_IsObject(member) ||
                string_from(member, "id")[0] == '\0') {
                continue;
            }
            light_from(member, room.members[room.member_count++]);
        }
        if (lights.effect_count == 0) {
            const cJSON* effect = nullptr;
            cJSON_ArrayForEach(effect, cJSON_GetObjectItemCaseSensitive(item, "effects")) {
                if (lights.effect_count < lights.effects.size() && cJSON_IsString(effect) &&
                    effect->valuestring != nullptr && effect->valuestring[0] != '\0') {
                    lights.effects[lights.effect_count++].assign(effect->valuestring);
                }
            }
        }
    }
}

// Klawisz trackpada w słowniku `remote.send_command` integracji apple_tv.
const char* key_name(domain::NavKey key) {
    switch (key) {
        case domain::NavKey::Up: return "up";
        case domain::NavKey::Down: return "down";
        case domain::NavKey::Left: return "left";
        case domain::NavKey::Right: return "right";
        case domain::NavKey::Ok: return "select";
        // Apple TV nie ma osobnego „wstecz” — tę rolę pełni Menu.
        case domain::NavKey::Back:
        case domain::NavKey::Menu: return "menu";
        case domain::NavKey::Home: return "home";
        default: return nullptr;
    }
}

}  // namespace

bool HaClient::start(MqttBus& bus, const storage::MqttSettings& settings) {
    stop();
    if (!settings.home_assistant()) {
        ESP_LOGI(kTag, "brak identyfikatora pilota, Apple TV pozostaje niedostepne");
        return false;
    }

    settings_ = settings;
    last_revision_ = 0;
    if (events_ == nullptr) {
        // Każdy snapshot zastępuje poprzedni, więc cztery miejsca wystarczą; kolejka
        // leży w pamięci wewnętrznej, a zdarzenie ma ok. 1,7 kB.
        // Ze światłami (ADR 0014) zdarzenie ma ok. 3 kB — kolejka idzie do PSRAM,
        // pamięć wewnętrzna jest potrzebna stosom zadań.
        events_ = xQueueCreateWithCaps(4, sizeof(HaEvent), MALLOC_CAP_SPIRAM);
        if (events_ == nullptr) {
            events_ = xQueueCreate(4, sizeof(HaEvent));
        }
    }
    if (assembly_ == nullptr) {
        assembly_ = static_cast<char*>(heap_caps_malloc(kAssemblyCapacity, MALLOC_CAP_SPIRAM));
        if (assembly_ == nullptr) {
            assembly_ = static_cast<char*>(malloc(kAssemblyCapacity));
        }
    }

    const char* id = settings_.remote_id.c_str();
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s/%s/state", kTopicPrefix, id);
    topic_state_.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "%s/%s/command", kTopicPrefix, id);
    topic_command_.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "%s/%s/availability", kTopicPrefix, id);
    topic_availability_.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "%s/%s/channels", kTopicPrefix, id);
    topic_channels_.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "%s/%s/weather", kTopicPrefix, id);
    topic_weather_.assign(buffer);
    if (channels_lock_ == nullptr) {
        channels_lock_ = xSemaphoreCreateMutex();
    }
    // Testament (availability = offline) ustawia `MqttBus`: jedno połączenie, jeden testament.
    bus_ = &bus;
    handle_ = bus.handle();
    if (handle_ == nullptr) {
        return false;
    }
    bus.attach(&HaClient::on_mqtt_event, this);
    return true;
}

void HaClient::stop() {
    handle_ = nullptr;
    connected_ = false;
    if (events_ != nullptr) {
        xQueueReset(events_);
    }
}

bool HaClient::poll(HaEvent& out) {
    return events_ != nullptr && xQueueReceive(events_, &out, 0) == pdTRUE;
}

bool HaClient::take_channels(domain::ChannelList& out) {
    if (channels_lock_ == nullptr || xSemaphoreTake(channels_lock_, 0) != pdTRUE) {
        return false;
    }
    const bool fresh = channels_fresh_;
    if (fresh) {
        const uint32_t revision = out.revision;
        out = channels_ready_;
        out.revision = revision + 1;
        channels_fresh_ = false;
    }
    xSemaphoreGive(channels_lock_);
    return fresh;
}

bool HaClient::take_weather(domain::Weather& out) {
    if (channels_lock_ == nullptr || xSemaphoreTake(channels_lock_, 0) != pdTRUE) {
        return false;
    }
    const bool fresh = weather_fresh_;
    if (fresh) {
        out = weather_ready_;
        weather_fresh_ = false;
    }
    xSemaphoreGive(channels_lock_);
    return fresh;
}

void HaClient::handle_weather(const char* data, int length) {
    cJSON* root = cJSON_ParseWithLength(data, static_cast<size_t>(length));
    if (root == nullptr) {
        ESP_LOGW(kTag, "pogoda nie jest poprawnym JSON-em");
        return;
    }
    const char* condition = string_from(root, "condition");
    const cJSON* temperature = cJSON_GetObjectItemCaseSensitive(root, "temperature");
    if (condition[0] != '\0' && cJSON_IsNumber(temperature) && channels_lock_ != nullptr &&
        xSemaphoreTake(channels_lock_, pdMS_TO_TICKS(100)) == pdTRUE) {
        weather_ready_.known = true;
        weather_ready_.condition.assign(condition);
        weather_ready_.temperature_tenths =
            static_cast<int16_t>(std::lround(temperature->valuedouble * 10.0));
        weather_ready_.night =
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "night")) != 0;
        // Sekcja `indoor` (ADR 0015) jest opcjonalna: bridge bez niej — brak kafla „w domu”.
        const cJSON* indoor = cJSON_GetObjectItemCaseSensitive(root, "indoor");
        const cJSON* inside = cJSON_GetObjectItemCaseSensitive(indoor, "temperature");
        const cJSON* humidity = cJSON_GetObjectItemCaseSensitive(indoor, "humidity");
        weather_ready_.indoor_known = cJSON_IsNumber(inside) != 0;
        weather_ready_.indoor_tenths =
            weather_ready_.indoor_known ? static_cast<int16_t>(std::lround(inside->valuedouble * 10.0)) : 0;
        weather_ready_.humidity_known = cJSON_IsNumber(humidity) != 0;
        weather_ready_.humidity = weather_ready_.humidity_known
            ? static_cast<uint8_t>(std::clamp(std::lround(humidity->valuedouble), 0L, 100L)) : 0;
        weather_ready_.received_ms = core::now_ms();
        weather_ready_.revision += 1;
        weather_fresh_ = true;
        xSemaphoreGive(channels_lock_);
        ESP_LOGI(kTag, "pogoda: %s %d", condition, static_cast<int>(weather_ready_.temperature_tenths));
    }
    cJSON_Delete(root);
}

void HaClient::handle_channels(const char* data, int length) {
    cJSON* root = cJSON_ParseWithLength(data, static_cast<size_t>(length));
    if (root == nullptr) {
        ESP_LOGW(kTag, "lista kanalow nie jest poprawnym JSON-em");
        return;
    }
    // Zadanie UI bierze muteks bez czekania, więc na czas składania listy
    // po prostu odbierze ją przy następnym obiegu.
    if (channels_lock_ == nullptr || xSemaphoreTake(channels_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
        cJSON_Delete(root);
        return;
    }
    domain::ChannelList& list = channels_ready_;
    list.count = 0;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(root, "channels")) {
        const char* id = string_from(item, "id");
        if (list.count >= list.items.size() || id[0] == '\0') {
            continue;
        }
        domain::Channel& channel = list.items[list.count++];
        channel.id.assign(id);
        channel.number.assign(string_from(item, "number"));
        channel.name.assign(string_from(item, "name"));
    }
    channels_fresh_ = true;
    const unsigned count = static_cast<unsigned>(list.count);
    xSemaphoreGive(channels_lock_);
    cJSON_Delete(root);
    ESP_LOGI(kTag, "lista kanalow: %u", count);
}

void HaClient::emit(const HaEvent& event) {
    if (events_ != nullptr) {
        xQueueSend(events_, &event, pdMS_TO_TICKS(100));
    }
}

void HaClient::publish_raw(const char* topic, const char* payload, int qos, bool retain) {
    if (handle_ == nullptr || !connected_) {
        return;
    }
    // Wysyła zadanie wysyłki wspólnego połączenia: nie czekamy na brokera w zadaniu UI.
    bus_->publish(topic, payload, qos, retain);
}

void HaClient::publish_command(const char* action, const char* args_json, int qos) {
    char id[17];
    std::snprintf(id, sizeof(id), "%08x%08x", static_cast<unsigned>(esp_random()),
                  static_cast<unsigned>(esp_random()));
    char payload[320];
    std::snprintf(payload, sizeof(payload), "{\"id\":\"%s\",\"action\":\"%s\",\"args\":%s}", id,
                  action, args_json);
    publish_raw(topic_command_.c_str(), payload, qos, false);
}

void HaClient::request_sync() {
    publish_command("remote.sync.request", "{}");
}

void HaClient::publish_intent(const domain::UiIntent& intent) {
    using domain::ControlId;

    // Tekst z klawiatury i bundle id idą przez cJSON, żeby cudzysłów albo
    // ukośnik nie rozbił payloadu.
    cJSON* args = cJSON_CreateObject();
    const char* action = nullptr;
    switch (intent.control) {
        case ControlId::AtvPower:
            action = "appletv.power.set";
            cJSON_AddBoolToObject(args, "on", intent.flag);
            break;
        case ControlId::AtvApp:
            action = "appletv.app.launch";
            cJSON_AddStringToObject(args, "app_id", intent.payload.c_str());
            break;
        case ControlId::TvChannel:
            // Glasshouse przełącza kanał wyłącznie po HTTP, więc robi to bridge
            // w Home Assistancie (ADR 0008). Skrót podaje nazwę z listy tunera,
            // menu kanałów — identyfikator, bo nazwy się powtarzają.
            action = "tv.channel.open";
            cJSON_AddStringToObject(args, "name", intent.text.c_str());
            if (!intent.payload.empty()) {
                cJSON_AddStringToObject(args, "id", intent.payload.c_str());
            }
            break;
        case ControlId::ClimatePower:
            action = "climate.power.set";
            cJSON_AddBoolToObject(args, "on", intent.flag);
            break;
        case ControlId::ClimateMode:
            action = "climate.mode.set";
            cJSON_AddStringToObject(args, "mode", intent.text.c_str());
            break;
        case ControlId::ClimateTarget:
            action = "climate.target.set";
            cJSON_AddNumberToObject(args, "temperature", static_cast<double>(intent.number) / 10.0);
            break;
        case ControlId::ClimateFan:
            action = "climate.fan.set";
            cJSON_AddStringToObject(args, "mode", intent.text.c_str());
            break;
        case ControlId::ClimateSwing:
            action = "climate.swing.set";
            cJSON_AddStringToObject(args, "mode", intent.text.c_str());
            break;
        case ControlId::ClimateSwingH:
            action = "climate.swing_horizontal.set";
            cJSON_AddStringToObject(args, "mode", intent.text.c_str());
            break;
        // Oświetlenie (ADR 0014): identyfikator światła z sekcji `lights`.
        case ControlId::LightPower:
            action = "light.power.set";
            cJSON_AddStringToObject(args, "id", intent.target.c_str());
            cJSON_AddBoolToObject(args, "on", intent.flag);
            break;
        case ControlId::LightBrightness:
            action = "light.brightness.set";
            cJSON_AddStringToObject(args, "id", intent.target.c_str());
            cJSON_AddNumberToObject(args, "percent", intent.value);
            break;
        case ControlId::LightKelvin:
            action = "light.color_temp.set";
            cJSON_AddStringToObject(args, "id", intent.target.c_str());
            cJSON_AddNumberToObject(args, "kelvin", intent.number);
            break;
        case ControlId::LightEffect:
            action = "light.effect.set";
            cJSON_AddStringToObject(args, "id", intent.target.c_str());
            cJSON_AddStringToObject(args, "effect", intent.payload.c_str());
            break;
        case ControlId::AtvKeyboard:
            if (intent.payload.empty()) {
                action = "appletv.keyboard.clear";
            } else {
                action = "appletv.keyboard.set";
                cJSON_AddStringToObject(args, "text", intent.payload.c_str());
            }
            break;
        default:
            if (intent.kind == domain::IntentKind::Navigate && intent.target == "appletv") {
                if (const char* key = key_name(intent.key)) {
                    action = "appletv.navigate";
                    cJSON_AddStringToObject(args, "key", key);
                    // Zwykłe stuknięcie jest domyślne i nie trafia do payloadu.
                    if (intent.press == domain::Press::Double) {
                        cJSON_AddStringToObject(args, "action", "double");
                    } else if (intent.press == domain::Press::Hold) {
                        cJSON_AddStringToObject(args, "action", "hold");
                    }
                }
            }
            break;
    }

    if (action != nullptr) {
        // Nawigacja to chwilowe zdarzenie: QoS 0, bez ponowień. Klawisz ponowiony
        // po sekundzie poruszyłby Apple TV, gdy palca już nie ma na gładziku.
        const int qos = intent.kind == domain::IntentKind::Navigate ? 0 : 1;
        char* json = cJSON_PrintUnformatted(args);
        if (json != nullptr) {
            publish_command(action, json, qos);
            cJSON_free(json);
        }
    }
    cJSON_Delete(args);
}

void HaClient::handle_payload(const char* data, int length) {
    cJSON* root = cJSON_ParseWithLength(data, static_cast<size_t>(length));
    if (root == nullptr) {
        ESP_LOGW(kTag, "snapshot nie jest poprawnym JSON-em");
        return;
    }

    const cJSON* revision_node = cJSON_GetObjectItemCaseSensitive(root, "revision");
    const uint32_t revision =
        cJSON_IsNumber(revision_node) ? static_cast<uint32_t>(revision_node->valuedouble) : 0;
    // Równa rewizja to powtórka. Niższa po połączeniu oznacza, że licznik
    // w Home Assistancie ruszył od nowa (restart HA) — bez tego Apple TV
    // zamarzłoby do restartu pilota.
    if (revision == last_revision_) {
        cJSON_Delete(root);
        return;
    }
    if (revision < last_revision_) {
        ESP_LOGI(kTag, "rewizja HA cofnela sie (%u -> %u), licznik od nowa",
                 static_cast<unsigned>(last_revision_), static_cast<unsigned>(revision));
    }
    last_revision_ = revision;

    HaEvent& event = outgoing_;
    event.kind = HaEvent::Kind::Snapshot;
    domain::AppleTvState& atv = event.appletv;
    atv = domain::AppleTvState{};

    // Bridge bez sekcji `appletv` to bridge sprzed ADR 0007: Apple TV zostaje
    // niedostępne, a ekran mówi o tym wprost.
    if (const cJSON* node = cJSON_GetObjectItemCaseSensitive(root, "appletv");
        cJSON_IsObject(node)) {
        atv.available = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(node, "available")) != 0;
        atv.power = power_from(cJSON_GetObjectItemCaseSensitive(node, "power"));
        atv.app_id.assign(string_from(node, "app_id"));
        atv.keyboard_active =
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(node, "keyboard_active")) != 0;

        const cJSON* apps = cJSON_GetObjectItemCaseSensitive(node, "apps");
        const cJSON* item = nullptr;
        cJSON_ArrayForEach(item, apps) {
            if (atv.app_count >= atv.apps.size() || !cJSON_IsObject(item)) {
                continue;
            }
            const char* id = string_from(item, "id");
            if (id[0] == '\0') {
                continue;
            }
            auto& app = atv.apps[atv.app_count++];
            app.id.assign(id);
            app.label.assign(string_from(item, "label"));
        }
    }

    climate_from(cJSON_GetObjectItemCaseSensitive(root, "climate"), event.climate);
    lights_from(cJSON_GetObjectItemCaseSensitive(root, "lights"), event.lights);

    cJSON_Delete(root);
    // Jedna linia na snapshot: bez niej z logu nie da się odróżnić „HA milczy”
    // od „HA mówi, że Apple TV jest niedostępne”.
    ESP_LOGI(kTag, "snapshot rewizja %u: apple tv %s, aplikacji %u, klima %s %s %d, swiatel %u",
             static_cast<unsigned>(revision), atv.available ? "dostepne" : "niedostepne",
             static_cast<unsigned>(atv.app_count),
             event.climate.available ? "dostepna" : "niedostepna",
             event.climate.hvac_mode.c_str(), static_cast<int>(event.climate.target_tenths),
             static_cast<unsigned>(event.lights.count));
    emit(event);
}

void HaClient::on_mqtt_event(void* self_raw, const char*, int32_t id, void* data) {
    auto* self = static_cast<HaClient*>(self_raw);
    auto* event = static_cast<esp_mqtt_event_handle_t>(data);
    if (self->handle_ == nullptr) {
        return;  // odpięty: połączenie jeszcze gaśnie
    }

    switch (static_cast<esp_mqtt_event_id_t>(id)) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(kTag, "polaczono z brokerem");
            self->connected_ = true;
            // Nowa sesja: retained snapshot przyjdzie na nowo, a licznik HA
            // mógł w międzyczasie ruszyć od zera.
            self->last_revision_ = 0;
            esp_mqtt_client_subscribe(event->client, self->topic_state_.c_str(), 1);
            esp_mqtt_client_subscribe(event->client, self->topic_channels_.c_str(), 1);
            esp_mqtt_client_subscribe(event->client, self->topic_weather_.c_str(), 1);
            self->publish_raw(self->topic_availability_.c_str(), "online", 1, true);
            // Po wejściu prosimy o świeży snapshot, zgodnie z ADR 0005, i o listę
            // kanałów — retained mogła zostać sprzed ponownego wyszukania kanałów.
            self->request_sync();
            self->publish_command("tv.channels.request", "{}");
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
        case MQTT_EVENT_ERROR: {
            // Tylko utrata działającego połączenia: próby sprzed startu Wi-Fi
            // padają co 1,5 s i zalałyby log.
            if (self->connected_) {
                ESP_LOGW(kTag, "utracono polaczenie z brokerem");
            }
            self->connected_ = false;
            HaEvent& offline = self->outgoing_;
            offline.kind = HaEvent::Kind::Offline;
            self->emit(offline);
            break;
        }
        case MQTT_EVENT_DATA: {
            if (self->assembly_ == nullptr) {
                break;
            }
            // Snapshot bywa dłuższy niż jedna ramka: sklejamy po przesunięciu.
            if (event->current_data_offset == 0) {
                self->assembly_len_ = 0;
                self->assembly_total_ = event->total_data_len;
                char topic[96];
                const int length = event->topic_len < static_cast<int>(sizeof(topic)) - 1
                                       ? event->topic_len
                                       : static_cast<int>(sizeof(topic)) - 1;
                std::memcpy(topic, event->topic, length);
                topic[length] = '\0';
                self->assembly_topic_.assign(topic);
                // Wspólne połączenie: tematy Z906 i Glasshouse nie są nasze.
                self->assembly_mine_ = self->assembly_topic_ == self->topic_state_ ||
                                       self->assembly_topic_ == self->topic_channels_ ||
                                       self->assembly_topic_ == self->topic_weather_;
            }
            if (!self->assembly_mine_) {
                break;
            }
            if (self->assembly_len_ + event->data_len <= kAssemblyCapacity) {
                std::memcpy(self->assembly_ + self->assembly_len_, event->data, event->data_len);
                self->assembly_len_ += event->data_len;
            }
            if (self->assembly_len_ >= self->assembly_total_ && self->assembly_total_ > 0) {
                if (self->assembly_topic_ == self->topic_channels_.c_str()) {
                    self->handle_channels(self->assembly_, self->assembly_len_);
                } else if (self->assembly_topic_ == self->topic_weather_.c_str()) {
                    self->handle_weather(self->assembly_, self->assembly_len_);
                } else {
                    self->handle_payload(self->assembly_, self->assembly_len_);
                }
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
