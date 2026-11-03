#include "transport/wifi_service.hpp"

#include <algorithm>
#include <cstring>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace transport {
namespace {

constexpr char kTag[] = "wifi";

// Bity sygnalizujące zadaniu roboczemu, co przyszło z pętli zdarzeń ESP-IDF.
constexpr EventBits_t kGotIp = BIT0;
constexpr EventBits_t kDisconnected = BIT1;
constexpr EventBits_t kScanDone = BIT2;

constexpr int kConnectTimeoutMs = 20000;
constexpr int kScanTimeoutMs = 15000;

// Powód rozłączenia tłumaczymy na coś, co da się pokazać na ekranie.
WifiError error_from_reason(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            return WifiError::BadPassword;
        case WIFI_REASON_NO_AP_FOUND:
            return WifiError::NotFound;
        default:
            return WifiError::Other;
    }
}

uint8_t g_last_reason = 0;

}  // namespace

struct WifiService::Request {
    enum class Kind : uint8_t { Scan, Connect, Disconnect } kind = Kind::Scan;
    Ssid ssid;
    Password password;
};

void WifiService::on_wifi_event(void* self_raw, const char* base, int32_t id, void* data) {
    auto* self = static_cast<WifiService*>(self_raw);

    if (std::strcmp(base, WIFI_EVENT) == 0) {
        if (id == WIFI_EVENT_SCAN_DONE) {
            xEventGroupSetBits(self->signals_, kScanDone);
            return;
        }
        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            const auto* info = static_cast<wifi_event_sta_disconnected_t*>(data);
            g_last_reason = info != nullptr ? info->reason : 0;
            xEventGroupSetBits(self->signals_, kDisconnected);
        }
        return;
    }

    if (std::strcmp(base, IP_EVENT) == 0 && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(self->signals_, kGotIp);
    }
}

uint8_t WifiService::last_reason() const {
    return g_last_reason;
}

void WifiService::start() {
    if (task_ != nullptr) {
        return;
    }
    requests_ = xQueueCreate(4, sizeof(Request));
    events_ = xQueueCreate(8, sizeof(WifiEvent));
    signals_ = xEventGroupCreate();
    // Stos z zapasem: każde zlecenie to RPC po SDIO do koprocesora.
    xTaskCreate(&WifiService::task_entry, "wifi_svc", 6144, this, 5, &task_);
}

void WifiService::task_entry(void* self) {
    static_cast<WifiService*>(self)->run();
}

void WifiService::scan() {
    Request request;
    request.kind = Request::Kind::Scan;
    xQueueSend(requests_, &request, 0);
}

void WifiService::connect(const Ssid& ssid, const Password& password) {
    Request request;
    request.kind = Request::Kind::Connect;
    request.ssid = ssid;
    request.password = password;
    xQueueSend(requests_, &request, 0);
}

void WifiService::disconnect() {
    Request request;
    request.kind = Request::Kind::Disconnect;
    xQueueSend(requests_, &request, 0);
}

bool WifiService::poll(WifiEvent& out) {
    return events_ != nullptr && xQueueReceive(events_, &out, 0) == pdTRUE;
}

void WifiService::publish(WifiPhase phase) {
    phase_ = phase;
    WifiEvent event;
    event.kind = WifiEvent::Kind::Phase;
    event.phase = phase;
    xQueueSend(events_, &event, pdMS_TO_TICKS(50));
}

void WifiService::publish_scan(std::size_t count) {
    WifiEvent event;
    event.kind = WifiEvent::Kind::ScanDone;
    event.phase = phase_;
    event.ap_count = count;
    xQueueSend(events_, &event, pdMS_TO_TICKS(50));
}

void WifiService::do_scan() {
    const bool was_connected = connected_;
    publish(WifiPhase::Scanning);

    // Skan blokujący nie przechodzi przez RPC do koprocesora — wraca natychmiast
    // z zerem wyników. Zlecamy nieblokująco i czekamy na zdarzenie SCAN_DONE.
    xEventGroupClearBits(signals_, kScanDone);
    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = false;

    // Koprocesor przy starcie sam próbuje łączyć się z siecią zapamiętaną we własnym
    // NVS i odrzuca wtedy skan z ESP_ERR_WIFI_STATE. Przerywamy próbę i ponawiamy.
    esp_err_t started = ESP_FAIL;
    for (int attempt = 0; attempt < 3; ++attempt) {
        started = esp_wifi_scan_start(&scan_config, false);
        if (started == ESP_OK || started != ESP_ERR_WIFI_STATE) {
            break;
        }
        // Stan zajęty bierze się z próby łączenia. Gdy łącze już stoi, nie wolno
        // go zrywać dla samego skanu — wtedy tylko czekamy.
        if (!connected_) {
            esp_wifi_disconnect();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (started != ESP_OK) {
        ESP_LOGW(kTag, "skan nie ruszyl: %s", esp_err_to_name(started));
        ap_count_ = 0;
        publish(was_connected ? WifiPhase::Connected : WifiPhase::Idle);
        publish_scan(0);
        return;
    }

    const EventBits_t bits =
        xEventGroupWaitBits(signals_, kScanDone, pdTRUE, pdFALSE, pdMS_TO_TICKS(kScanTimeoutMs));
    if ((bits & kScanDone) == 0) {
        ESP_LOGW(kTag, "skan nie zakonczyl sie w czasie");
        esp_wifi_scan_stop();
        ap_count_ = 0;
        publish(was_connected ? WifiPhase::Connected : WifiPhase::Idle);
        publish_scan(0);
        return;
    }

    uint16_t found = 0;
    esp_wifi_scan_get_ap_num(&found);
    found = std::min<uint16_t>(found, kMaxAccessPoints * 2);

    auto* records = static_cast<wifi_ap_record_t*>(
        heap_caps_malloc(sizeof(wifi_ap_record_t) * found, MALLOC_CAP_DEFAULT));
    if (records == nullptr) {
        ap_count_ = 0;
        publish(WifiPhase::Idle);
        publish_scan(0);
        return;
    }
    esp_wifi_scan_get_ap_records(&found, records);

    // Ta sama sieć bywa widziana na kilku kanałach — zostawiamy najmocniejszy wpis.
    std::size_t count = 0;
    for (uint16_t i = 0; i < found && count < kMaxAccessPoints; ++i) {
        const char* ssid = reinterpret_cast<const char*>(records[i].ssid);
        if (ssid[0] == '\0') {
            continue;  // sieć ukryta — nie ma czego pokazać na liście
        }
        bool duplicate = false;
        for (std::size_t j = 0; j < count; ++j) {
            if (access_points_[j].ssid == ssid) {
                duplicate = true;
                if (records[i].rssi > access_points_[j].rssi) {
                    access_points_[j].rssi = records[i].rssi;
                }
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        access_points_[count].ssid.assign(ssid);
        access_points_[count].rssi = records[i].rssi;
        access_points_[count].secured = records[i].authmode != WIFI_AUTH_OPEN;
        ++count;
    }
    heap_caps_free(records);

    std::sort(access_points_, access_points_ + count,
              [](const AccessPoint& a, const AccessPoint& b) { return a.rssi > b.rssi; });
    ap_count_ = count;

    // Skan nie zmienia stanu połączenia: wracamy do tego, w którym byliśmy.
    publish(was_connected ? WifiPhase::Connected : WifiPhase::Idle);
    publish_scan(count);
    ESP_LOGI(kTag, "skan: %u sieci", static_cast<unsigned>(count));
}

void WifiService::do_connect(const Ssid& ssid, const Password& password) {
    error_ = WifiError::None;
    publish(WifiPhase::Connecting);

    wifi_config_t config = {};
    // Oba pola są tablicami bajtów bez zakończenia, a struktura jest wyzerowana,
    // więc kopiujemy dokładnie tyle, ile wynosi długość.
    std::memcpy(config.sta.ssid, ssid.c_str(),
                std::min(std::strlen(ssid.c_str()), sizeof(config.sta.ssid)));
    std::memcpy(config.sta.password, password.c_str(),
                std::min(std::strlen(password.c_str()), sizeof(config.sta.password)));
    config.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    esp_wifi_disconnect();
    xEventGroupClearBits(signals_, kGotIp | kDisconnected);
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        error_ = WifiError::Other;
        publish(WifiPhase::Failed);
        return;
    }

    const EventBits_t bits = xEventGroupWaitBits(signals_, kGotIp | kDisconnected, pdTRUE, pdFALSE,
                                                 pdMS_TO_TICKS(kConnectTimeoutMs));
    if ((bits & kGotIp) != 0) {
        current_ssid_ = ssid;
        wifi_ap_record_t record = {};
        if (esp_wifi_sta_get_ap_info(&record) == ESP_OK) {
            current_rssi_ = record.rssi;
        }
        // Nazwa sieci jest prywatna — do logu trafia wyłącznie fakt połączenia.
        connected_ = true;
        ESP_LOGI(kTag, "polaczono, rssi %d dBm", static_cast<int>(current_rssi_));
        publish(WifiPhase::Connected);
        return;
    }

    connected_ = false;
    error_ = (bits & kDisconnected) != 0 ? error_from_reason(g_last_reason) : WifiError::NoAddress;
    esp_wifi_disconnect();
    ESP_LOGW(kTag, "polaczenie nieudane, powod %u", static_cast<unsigned>(g_last_reason));
    publish(WifiPhase::Failed);
}

// Zerwanie poza procedurą łączenia: ktoś wyłączył router, zasięg spadł.
// Zgłaszamy to wyżej, a ponowienie jest decyzją kontrolera, nie sterownika.
void WifiService::note_disconnect() {
    if (!connected_) {
        return;
    }
    connected_ = false;
    current_rssi_ = 0;
    error_ = error_from_reason(g_last_reason);
    ESP_LOGW(kTag, "lacze zerwane, powod %u", static_cast<unsigned>(g_last_reason));
    publish(WifiPhase::Idle);
}

void WifiService::handle(const Request& request) {
    switch (request.kind) {
        case Request::Kind::Scan:
            do_scan();
            break;
        case Request::Kind::Connect:
            do_connect(request.ssid, request.password);
            break;
        case Request::Kind::Disconnect:
            esp_wifi_disconnect();
            connected_ = false;
            current_ssid_.clear();
            publish(WifiPhase::Idle);
            break;
    }
}

void WifiService::run() {
    esp_err_t ready = nvs_flash_init();
    if (ready == ESP_ERR_NVS_NO_FREE_PAGES || ready == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ready = nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_init nie powiodlo sie");
        publish(WifiPhase::Failed);
        vTaskDelete(nullptr);
        return;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiService::on_wifi_event,
                                        this, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiService::on_wifi_event,
                                        this, nullptr);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);  // dane dostępowe trzyma nasz magazyn, nie sterownik
    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start nie powiodlo sie");
        publish(WifiPhase::Failed);
        vTaskDelete(nullptr);
        return;
    }

    // Dane dostępowe trzyma nasz magazyn, nie sterownik w koprocesorze. Fabryczne
    // demo zostawiło we flashu C6 własny wpis i bez tego pilot od startu wisiałby
    // na cudzej próbie połączenia.
    esp_wifi_disconnect();

    ESP_LOGI(kTag, "radio gotowe (ESP32-C6 przez SDIO)");
    publish(WifiPhase::Idle);

    while (true) {
        Request request;
        if (xQueueReceive(requests_, &request, pdMS_TO_TICKS(500)) == pdTRUE) {
            handle(request);
            continue;
        }
        // Cisza na kolejce to dobry moment, żeby zauważyć zerwane łącze.
        if ((xEventGroupClearBits(signals_, kDisconnected) & kDisconnected) != 0) {
            note_disconnect();
        }
    }
}

}  // namespace transport
