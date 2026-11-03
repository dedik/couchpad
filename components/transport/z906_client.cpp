#include "transport/z906_client.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/config.hpp"
#include "esp_log.h"
#include "mqtt_client.h"

namespace transport {
namespace {

constexpr char kTag[] = "z906";

bool is_on(const char* value, int length) {
    return length >= 2 && (std::strncmp(value, "ON", 2) == 0 || std::strncmp(value, "on", 2) == 0);
}

uint8_t percent_of(const char* value, int length) {
    char buffer[8] = {};
    const int copy = length < static_cast<int>(sizeof(buffer) - 1) ? length
                                                                   : static_cast<int>(sizeof(buffer) - 1);
    std::memcpy(buffer, value, copy);
    const int parsed = std::atoi(buffer);
    return static_cast<uint8_t>(parsed < 0 ? 0 : (parsed > 100 ? 100 : parsed));
}

void assign(domain::Label& label, const char* value, int length) {
    char buffer[32] = {};
    const int copy = length < static_cast<int>(sizeof(buffer) - 1) ? length
                                                                   : static_cast<int>(sizeof(buffer) - 1);
    std::memcpy(buffer, value, copy);
    label.assign(buffer);
}

// Kontroler nazywa kanał centralny po brytyjsku. Łatwo się pomylić, więc nazwa
// żyje w jednym miejscu.
constexpr char kCentreLeaf[] = "centre";

const char* leaf_for(domain::ControlId control) {
    switch (control) {
        case domain::ControlId::ZPower: return "power";
        case domain::ControlId::ZMute: return "mute";
        case domain::ControlId::ZVolume: return "volume";
        case domain::ControlId::ZCenter: return kCentreLeaf;
        case domain::ControlId::ZSubwoofer: return "subwoofer";
        case domain::ControlId::ZRear: return "rear";
        case domain::ControlId::ZSource: return "source";
        case domain::ControlId::ZEffect: return "effect";
        default: return nullptr;
    }
}

}  // namespace

bool Z906Client::start(MqttBus& bus, const storage::MqttSettings& settings) {
    if (!settings.complete()) {
        ESP_LOGW(kTag, "ustawienia niekompletne");
        return false;
    }
    stop();

    settings_ = settings;
    if (events_ == nullptr) {
        // Retained stan przychodzi kilkunastoma wiadomościami naraz, a każda
        // rodzi nowy snapshot — kolejka musi to przyjąć bez gubienia.
        events_ = xQueueCreate(12, sizeof(Z906Event));
    }

    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "z906/%s/state/", settings_.z906_id.c_str());
    topic_state_prefix_.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "z906/%s/command/", settings_.z906_id.c_str());
    topic_command_prefix_.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "z906/%s/availability", settings_.z906_id.c_str());
    topic_availability_.assign(buffer);

    bus_ = &bus;
    handle_ = bus.handle();
    if (handle_ == nullptr) {
        emit_phase(domain::LinkPhase::Error, domain::LinkStep::Mqtt);
        return false;
    }
    bus.attach(&Z906Client::on_mqtt_event, this);

    ready_ = false;
    connected_ = false;
    emit_phase(domain::LinkPhase::Connecting, domain::LinkStep::Mqtt);
    return true;
}

void Z906Client::stop() {
    handle_ = nullptr;
    connected_ = false;
}

bool Z906Client::poll(Z906Event& out) {
    return events_ != nullptr && xQueueReceive(events_, &out, 0) == pdTRUE;
}

void Z906Client::emit_phase(domain::LinkPhase phase, domain::LinkStep step) {
    Z906Event event;
    event.kind = Z906Event::Kind::Phase;
    event.phase = phase;
    event.step = step;
    xQueueSend(events_, &event, pdMS_TO_TICKS(100));
}

void Z906Client::emit_snapshot() {
    ++revision_;

    Z906Event event;
    event.kind = Z906Event::Kind::Snapshot;
    event.revision = revision_;
    event.z906 = state_;
    event.z906.available = available_;
    // Listy wejść i efektów są stałe dla modelu: domain/z906_options.hpp, nie snapshot.
    // Pozostałe urządzenia dokłada LiveSource ze swoich źródeł.
    xQueueSend(events_, &event, pdMS_TO_TICKS(100));

    if (!ready_ && available_) {
        ready_ = true;
        ESP_LOGI(kTag, "stan kompletny, rewizja %u", static_cast<unsigned>(revision_));
        emit_phase(domain::LinkPhase::Ready, domain::LinkStep::Sync);
    }
}

void Z906Client::apply_topic(const char* topic, int topic_len, const char* data, int data_len) {
    const int availability_len = static_cast<int>(std::strlen(topic_availability_.c_str()));
    if (topic_len == availability_len &&
        std::strncmp(topic, topic_availability_.c_str(), availability_len) == 0) {
        available_ = data_len >= 6 && std::strncmp(data, "online", 6) == 0;
        ESP_LOGI(kTag, "availability = %.*s", data_len, data);
        emit_snapshot();
        return;
    }

    const int prefix_len = static_cast<int>(std::strlen(topic_state_prefix_.c_str()));
    if (topic_len <= prefix_len ||
        std::strncmp(topic, topic_state_prefix_.c_str(), prefix_len) != 0) {
        return;
    }

    const char* leaf = topic + prefix_len;
    const int leaf_len = topic_len - prefix_len;
    auto leaf_is = [&](const char* name) {
        return leaf_len == static_cast<int>(std::strlen(name)) &&
               std::strncmp(leaf, name, leaf_len) == 0;
    };

    // Kontroler powtarza komplet stanu co kilka sekund. Bez tego porównania
    // każde powtórzenie dawałoby dziewięć nowych snapshotów i tyleż przerysowań,
    // a palec na tarczy walczyłby z własnym, nieświeżym echem.
    const domain::Z906State before = state_;

    if (leaf_is("power")) {
        state_.power = is_on(data, data_len) ? domain::Power::On : domain::Power::Off;
    } else if (leaf_is("mute")) {
        state_.muted = is_on(data, data_len);
    } else if (leaf_is("volume")) {
        state_.volume_percent = percent_of(data, data_len);
    } else if (leaf_is(kCentreLeaf)) {
        state_.center_percent = percent_of(data, data_len);
    } else if (leaf_is("subwoofer")) {
        state_.subwoofer_percent = percent_of(data, data_len);
    } else if (leaf_is("rear")) {
        state_.rear_percent = percent_of(data, data_len);
    } else if (leaf_is("source")) {
        assign(state_.source, data, data_len);
    } else if (leaf_is("effect")) {
        assign(state_.effect, data, data_len);
    } else if (leaf_is("decode")) {
        state_.decode_5_1 = is_on(data, data_len);
    } else {
        return;  // diagnostyka i last_command nie należą do ekranu sterowania
    }

    const bool changed = before.power != state_.power || before.muted != state_.muted ||
                         before.volume_percent != state_.volume_percent ||
                         before.center_percent != state_.center_percent ||
                         before.subwoofer_percent != state_.subwoofer_percent ||
                         before.rear_percent != state_.rear_percent ||
                         before.source != state_.source || before.effect != state_.effect ||
                         before.decode_5_1 != state_.decode_5_1;
    // Pierwszy komplet i każdy odebrany po żądaniu odświeżenia muszą przejść
    // nawet bez zmiany, bo to one zdejmują nakładkę łączenia.
    if (changed || !ready_ || resync_) {
        resync_ = false;
        emit_snapshot();
    }
}

void Z906Client::publish_value(const char* leaf, const char* value) {
    // Bez połączenia polecenie i tak by przepadło, a kolejka biblioteki
    // wysłałaby je dopiero po powrocie — z nieaktualną już wartością.
    if (handle_ == nullptr || !connected_) {
        return;
    }
    char topic[112];
    std::snprintf(topic, sizeof(topic), "%s%s", topic_command_prefix_.c_str(), leaf);
    // Wołane z zadania UI: wysyła zadanie wysyłki wspólnego połączenia, nie my.
    bus_->publish(topic, value, 1, false);
}

void Z906Client::publish_intent(const domain::UiIntent& intent) {
    const char* leaf = leaf_for(intent.control);
    if (leaf == nullptr) {
        return;  // TV, sceny i skróty nie mają tu odpowiednika
    }

    char value[40];
    switch (intent.control) {
        case domain::ControlId::ZPower:
        case domain::ControlId::ZMute:
            std::snprintf(value, sizeof(value), "%s", intent.flag ? "ON" : "OFF");
            break;
        case domain::ControlId::ZVolume:
        case domain::ControlId::ZCenter:
        case domain::ControlId::ZSubwoofer:
        case domain::ControlId::ZRear:
            std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(intent.value));
            break;
        default:
            std::snprintf(value, sizeof(value), "%s", intent.text.c_str());
            break;
    }
    publish_value(leaf, value);
}

void Z906Client::request_sync() {
    // Kontroler nie zna żądania synchronizacji — publikuje stan jako retained.
    // W MQTT 3.1.1 każdy SUBSCRIBE dostaje retained na nowo, więc ponowna
    // subskrypcja jest tym żądaniem. Broker odsyła komplet, a każdy temat
    // podnosi rewizję, więc interfejs dostaje świeży obraz.
    if (handle_ == nullptr) {
        return;
    }
    resync_ = true;
    char filter[112];
    std::snprintf(filter, sizeof(filter), "z906/%s/state/#", settings_.z906_id.c_str());
    esp_mqtt_client_subscribe(static_cast<esp_mqtt_client_handle_t>(handle_), filter, 1);
    esp_mqtt_client_subscribe(static_cast<esp_mqtt_client_handle_t>(handle_),
                              topic_availability_.c_str(), 1);
    ESP_LOGI(kTag, "prosze o swiezy stan");
}

void Z906Client::on_mqtt_event(void* self_raw, const char*, int32_t id, void* data) {
    auto* self = static_cast<Z906Client*>(self_raw);
    auto* event = static_cast<esp_mqtt_event_handle_t>(data);
    if (self->handle_ == nullptr) {
        return;  // odpięty: połączenie jeszcze gaśnie
    }

    switch (static_cast<esp_mqtt_event_id_t>(id)) {
        case MQTT_EVENT_CONNECTED: {
            self->error_ = MqttError::None;
            self->connected_ = true;
            char filter[112];
            std::snprintf(filter, sizeof(filter), "z906/%s/state/#",
                          self->settings_.z906_id.c_str());
            esp_mqtt_client_subscribe(event->client, filter, 1);
            esp_mqtt_client_subscribe(event->client, self->topic_availability_.c_str(), 1);
            ESP_LOGI(kTag, "polaczono, nasluchuje %s", filter);
            self->emit_phase(domain::LinkPhase::Connecting, domain::LinkStep::Sync);
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            self->connected_ = false;
            self->ready_ = false;
            self->emit_phase(domain::LinkPhase::Error, domain::LinkStep::Mqtt);
            break;
        case MQTT_EVENT_DATA:
            self->apply_topic(event->topic, event->topic_len, event->data, event->data_len);
            break;
        case MQTT_EVENT_ERROR: {
            const auto* failure = event->error_handle;
            if (failure != nullptr &&
                failure->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                switch (failure->connect_return_code) {
                    case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
                    case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
                        self->error_ = MqttError::NotAuthorized;
                        break;
                    case MQTT_CONNECTION_REFUSE_ID_REJECTED:
                    case MQTT_CONNECTION_REFUSE_PROTOCOL:
                        self->error_ = MqttError::Rejected;
                        break;
                    default:
                        self->error_ = MqttError::Other;
                        break;
                }
            } else {
                self->error_ = MqttError::Unreachable;
            }
            self->emit_phase(domain::LinkPhase::Error, domain::LinkStep::Mqtt);
            break;
        }
        default:
            break;
    }
}

}  // namespace transport
