#include "transport/mqtt_bus.hpp"

#include <cstdio>
#include <cstring>

#include "core/config.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mqtt_client.h"

namespace transport {
namespace {

constexpr char kTag[] = "mqtt";

// Największa wiadomość, jaką pilot odbiera w jednej ramce. Snapshot HA i telemetria
// Glasshouse są dłuższe i przychodzą w kawałkach, które klienci sklejają sami.
constexpr int kBufferSize = 4096;
// Kolejka wysyłki leży w pamięci wewnętrznej (FreeRTOS); osiem wiadomości to zapas
// na serię kroków gładzika. Stos zadania wysyłki jest w PSRAM: zadanie nie dotyka flasha.
constexpr UBaseType_t kQueueLength = 8;
constexpr uint32_t kSenderStack = 4096;

}  // namespace

bool MqttBus::open(const storage::MqttSettings& settings) {
    close();
    if (lock_ == nullptr) {
        lock_ = xSemaphoreCreateMutex();
        outgoing_ = xQueueCreate(kQueueLength, sizeof(Outgoing));
        xTaskCreatePinnedToCoreWithCaps(&MqttBus::sender_entry, "mqtt_tx", kSenderStack, this, 5,
                                        &sender_task_, tskNO_AFFINITY,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (settings.host.empty()) {
        return false;
    }
    settings_ = settings;

    // Identyfikator musi różnić się od kontrolera Z906 (`z906_<mac>`): broker
    // rozłącza drugą sesję o tym samym identyfikatorze.
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "pilot-%s",
                  !settings_.remote_id.empty() ? settings_.remote_id.c_str()
                                               : settings_.z906_id.c_str());
    client_id_.assign(buffer);

    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = settings_.host.c_str();
    config.broker.address.port = settings_.port;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    if (!settings_.username.empty()) {
        config.credentials.username = settings_.username.c_str();
        config.credentials.authentication.password = settings_.password.c_str();
    }
    config.credentials.client_id = client_id_.c_str();
    // Testament kontraktu v1: gdy pilot zniknie, Home Assistant dowie się o tym sam.
    if (!settings_.remote_id.empty()) {
        std::snprintf(buffer, sizeof(buffer), "multimedia_remote/v1/%s/availability",
                      settings_.remote_id.c_str());
        will_topic_.assign(buffer);
        config.session.last_will.topic = will_topic_.c_str();
        config.session.last_will.msg = "offline";
        config.session.last_will.qos = 1;
        config.session.last_will.retain = 1;
    }
    config.buffer.size = kBufferSize;
    config.network.reconnect_timeout_ms = core::config::kMqttReconnectMs;
    config.network.timeout_ms = core::config::kMqttNetworkTimeoutMs;

    handle_ = esp_mqtt_client_init(&config);
    if (handle_ == nullptr) {
        ESP_LOGE(kTag, "nie udalo sie utworzyc klienta");
        return false;
    }
    return true;
}

void MqttBus::attach(Handler handler, void* arg) {
    if (handle_ != nullptr) {
        esp_mqtt_client_register_event(static_cast<esp_mqtt_client_handle_t>(handle_),
                                       MQTT_EVENT_ANY, handler, arg);
    }
}

bool MqttBus::start() {
    if (handle_ == nullptr) {
        return false;
    }
    if (esp_mqtt_client_start(static_cast<esp_mqtt_client_handle_t>(handle_)) != ESP_OK) {
        ESP_LOGE(kTag, "nie udalo sie wystartowac klienta");
        close();
        return false;
    }
    return true;
}

void MqttBus::close() {
    if (handle_ == nullptr) {
        return;
    }
    // Zadanie wysyłki może być akurat w `publish` na tym kliencie — czekamy na nie.
    xSemaphoreTake(lock_, portMAX_DELAY);
    auto client = static_cast<esp_mqtt_client_handle_t>(handle_);
    handle_ = nullptr;
    xSemaphoreGive(lock_);
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    xQueueReset(outgoing_);
}

void MqttBus::publish(const char* topic, const char* payload, int qos, bool retain) {
    if (handle_ == nullptr || outgoing_ == nullptr) {
        return;
    }
    Outgoing message{};
    std::strncpy(message.topic, topic, kMaxTopic - 1);
    std::strncpy(message.payload, payload, kMaxPayload - 1);
    message.qos = static_cast<uint8_t>(qos);
    message.retain = retain;
    if (xQueueSend(outgoing_, &message, 0) != pdTRUE) {
        ESP_LOGW(kTag, "kolejka wysylki pelna, pomijam %s", topic);
    }
}

void MqttBus::sender_entry(void* self) {
    static_cast<MqttBus*>(self)->sender();
}

void MqttBus::sender() {
    while (true) {
        if (xQueueReceive(outgoing_, &sending_, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        xSemaphoreTake(lock_, portMAX_DELAY);
        if (handle_ != nullptr) {
            // Blokuje to zadanie, nie UI. Bez połączenia biblioteka sama odmawia.
            esp_mqtt_client_publish(static_cast<esp_mqtt_client_handle_t>(handle_),
                                    sending_.topic, sending_.payload, 0, sending_.qos,
                                    sending_.retain ? 1 : 0);
        }
        xSemaphoreGive(lock_);
    }
}

}  // namespace transport
