// Wi-Fi na telefonie należy do systemu: pilot nie skanuje ani nie łączy, tylko
// zgłasza „połączono”, gdy system ma trasę do sieci. Ekran Wi-Fi z firmware'u pokazuje
// wtedy sieć telefonu i nie ma czego wybierać.
//
// Implementuje ten sam nagłówek co transport/wifi_service.cpp płytki.

#include <cstring>

#include "esp_netif.h"
#include "freertos/queue.h"
#include "transport/wifi_service.hpp"

namespace transport {

void WifiService::start() {
    if (events_ == nullptr) {
        events_ = xQueueCreate(8, sizeof(WifiEvent));
    }
    esp_netif_ip_info_t info{};
    const bool online =
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &info) == ESP_OK &&
        info.ip.addr != 0;
    connected_ = online;
    phase_ = online ? WifiPhase::Connected : WifiPhase::Idle;
    error_ = WifiError::None;
    current_ssid_.assign(online ? "Sieć telefonu" : "");
    current_rssi_ = online ? -50 : 0;
    WifiEvent event;
    event.kind = WifiEvent::Kind::Phase;
    event.phase = phase_;
    xQueueSend(events_, &event, 0);
}

void WifiService::scan() {
    ap_count_ = 0;
    WifiEvent event;
    event.kind = WifiEvent::Kind::ScanDone;
    event.phase = phase_;
    event.ap_count = 0;
    if (events_ != nullptr) {
        xQueueSend(events_, &event, 0);
    }
}

void WifiService::connect(const Ssid&, const Password&) {
    // Siecią zarządza system telefonu.
}

void WifiService::disconnect() {}

bool WifiService::poll(WifiEvent& out) {
    return events_ != nullptr && xQueueReceive(events_, &out, 0) == pdTRUE;
}

uint8_t WifiService::last_reason() const { return 0; }

}  // namespace transport
