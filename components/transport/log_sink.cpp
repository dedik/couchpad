#include "transport/log_sink.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "core/config.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "mqtt_client.h"

namespace transport::log_sink {
namespace {

constexpr char kTag[] = "log_sink";

// Około 300 linii startu, zanim Wi-Fi i broker wstaną. Gdy bufor jest pełny,
// nowe linie przepadają — port szeregowy dostaje je zawsze.
constexpr std::size_t kBufferBytes = 32 * 1024;
constexpr std::size_t kLineMax = 240;

vprintf_like_t g_serial = nullptr;
RingbufHandle_t g_lines = nullptr;
TaskHandle_t g_sender = nullptr;
esp_mqtt_client_handle_t g_client = nullptr;
std::atomic<bool> g_connected{false};

core::Text<64> g_topic;
core::Text<48> g_client_id;
storage::MqttSettings g_settings;

// Linie samego klienta MQTT i jego gniazda nie wracają do MQTT: przy zerwanym
// łączu każda próba wysłania rodziłaby kolejną linię o błędzie.
bool from_network_stack(const char* line) {
    static const char* const kTags[] = {"mqtt_client:", "transport_base:", "esp-tls:",
                                        "outbox:", "log_sink:"};
    for (const char* tag : kTags) {
        if (std::strstr(line, tag) != nullptr) {
            return true;
        }
    }
    return false;
}

int hook(const char* format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    const int written = g_serial(format, args);

    // Linie z zadania wysyłającego pomijamy, inaczej log karmiłby sam siebie.
    if (g_lines != nullptr && xTaskGetCurrentTaskHandle() != g_sender) {
        char line[kLineMax];
        int length = std::vsnprintf(line, sizeof(line), format, copy);
        if (length > 0) {
            if (length >= static_cast<int>(sizeof(line))) {
                length = sizeof(line) - 1;
            }
            while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
                --length;
            }
            line[length] = '\0';
            if (length > 0 && !from_network_stack(line)) {
                xRingbufferSend(g_lines, line, length + 1, 0);
            }
        }
    }
    va_end(copy);
    return written;
}

void on_mqtt_event(void*, const char*, int32_t id, void*) {
    if (id == MQTT_EVENT_CONNECTED) {
        g_connected = true;
    } else if (id == MQTT_EVENT_DISCONNECTED || id == MQTT_EVENT_ERROR) {
        g_connected = false;
    }
}

bool connect_client();

void send_loop(void*) {
    // Stos TCP/IP podnosi zadanie usługi Wi-Fi. Klient MQTT ruszony wcześniej
    // kończył się asercją w lwIP („Invalid mbox”) i pętlą restartów.
    while (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (!connect_client()) {
        vTaskDelete(nullptr);
        return;
    }
    while (true) {
        if (!g_connected) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        std::size_t size = 0;
        auto* line = static_cast<char*>(xRingbufferReceive(g_lines, &size, pdMS_TO_TICKS(500)));
        if (line == nullptr) {
            continue;
        }
        esp_mqtt_client_enqueue(g_client, g_topic.c_str(), line, 0, 0, 0, true);
        vRingbufferReturnItem(g_lines, line);
    }
}

}  // namespace

void capture() {
    if (g_lines != nullptr) {
        return;
    }
    g_lines = xRingbufferCreateWithCaps(kBufferBytes, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    if (g_lines == nullptr) {
        return;
    }
    g_serial = esp_log_set_vprintf(&hook);
}

void start(const storage::MqttSettings& settings, const char* id) {
    if (g_lines == nullptr || g_sender != nullptr || settings.host.empty()) {
        return;
    }
    g_settings = settings;

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "pilot_debug/%s/log", id);
    g_topic.assign(buffer);
    std::snprintf(buffer, sizeof(buffer), "pilot-log-%s", id);
    g_client_id.assign(buffer);

    // Stos w PSRAM: zadanie tylko odkłada linie do kolejki MQTT i nie dotyka flasha.
    xTaskCreatePinnedToCoreWithCaps(&send_loop, "log_sink", 4096, nullptr, 1, &g_sender,
                                    tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

namespace {

bool connect_client() {
    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = g_settings.host.c_str();
    config.broker.address.port = g_settings.port;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    if (!g_settings.username.empty()) {
        config.credentials.username = g_settings.username.c_str();
        config.credentials.authentication.password = g_settings.password.c_str();
    }
    config.credentials.client_id = g_client_id.c_str();
    config.network.reconnect_timeout_ms = core::config::kMqttReconnectMs;
    config.network.timeout_ms = core::config::kMqttNetworkTimeoutMs;
    // Kolejka biblioteki nie może urosnąć bez końca, gdy broker nie odbiera.
    config.outbox.limit = 16 * 1024;

    g_client = esp_mqtt_client_init(&config);
    if (g_client == nullptr) {
        return false;
    }
    esp_mqtt_client_register_event(g_client, MQTT_EVENT_ANY, &on_mqtt_event, nullptr);
    esp_mqtt_client_start(g_client);
    ESP_LOGI(kTag, "log idzie tez do MQTT");
    return true;
}

}  // namespace

}  // namespace transport::log_sink
