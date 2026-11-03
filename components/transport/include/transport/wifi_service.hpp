#pragma once

// Wi-Fi na tej płytce idzie przez ESP32-C6: `esp_wifi_remote` podstawia zwykłe API
// `esp_wifi_*`, a `esp_hosted` dowozi je po SDIO. Piny i wersje są potwierdzone
// w docs/hardware/verified-board-map.md.
//
// Każde wywołanie do koprocesora to RPC po SDIO i potrafi trwać sekundy, więc usługa
// pracuje we własnym zadaniu, a wyniki oddaje kolejką. Interfejs nigdy nie czeka —
// ta sama zasada, co przy źródle snapshotów.

#include <cstddef>
#include <cstdint>

#include "core/text.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace transport {

inline constexpr std::size_t kMaxAccessPoints = 20;

using Ssid = core::Text<33>;      // 32 znaki plus zakończenie
using Password = core::Text<65>;  // WPA2 dopuszcza 63 znaki

struct AccessPoint {
    Ssid ssid;
    int8_t rssi = 0;
    bool secured = true;
};

enum class WifiPhase : uint8_t {
    Off,          // radio nieuruchomione
    Idle,         // gotowe, bez połączenia
    Scanning,
    Connecting,
    Connected,    // jest adres IP
    Failed,       // ostatnia próba nie powiodła się
};

struct WifiEvent {
    enum class Kind : uint8_t { Phase, ScanDone };
    Kind kind = Kind::Phase;
    WifiPhase phase = WifiPhase::Off;
    std::size_t ap_count = 0;
};

// Powód ostatniego niepowodzenia — do pokazania w interfejsie, nie do logu.
enum class WifiError : uint8_t { None, BadPassword, NotFound, NoAddress, Other };

class WifiService {
public:
    // Uruchamia zadanie robocze i radio. Wraca natychmiast.
    void start();

    // Zlecenia. Żadne nie blokuje wołającego.
    void scan();
    void connect(const Ssid& ssid, const Password& password);
    void disconnect();

    // Odbiór zdarzeń — wyłącznie z zadania UI.
    bool poll(WifiEvent& out);

    // Migawka wyników. Czytać po zdarzeniu `ScanDone`; tablica nie zmienia się
    // między zdarzeniami, bo pisze ją wyłącznie zadanie robocze przed ich wysłaniem.
    const AccessPoint* access_points() const { return access_points_; }
    std::size_t access_point_count() const { return ap_count_; }

    WifiPhase phase() const { return phase_; }
    WifiError error() const { return error_; }
    const Ssid& current_ssid() const { return current_ssid_; }
    int8_t current_rssi() const { return current_rssi_; }
    // Kod powodu ostatniego rozłączenia albo nieudanej próby (wifi_err_reason_t), 0 = brak.
    uint8_t last_reason() const;

private:
    struct Request;

    static void task_entry(void* self);
    void run();
    void handle(const Request& request);
    void do_scan();
    void note_disconnect();
    void do_connect(const Ssid& ssid, const Password& password);
    void publish(WifiPhase phase);
    void publish_scan(std::size_t count);

    static void on_wifi_event(void* self, const char* base, int32_t id, void* data);

    QueueHandle_t requests_ = nullptr;
    QueueHandle_t events_ = nullptr;
    TaskHandle_t task_ = nullptr;
    EventGroupHandle_t signals_ = nullptr;

    AccessPoint access_points_[kMaxAccessPoints]{};
    std::size_t ap_count_ = 0;

    // Faza zmienia się też na czas skanowania, więc osobno trzymamy fakt,
    // czy łącze stoi. Bez tego skan wygląda z zewnątrz jak rozłączenie.
    bool connected_ = false;
    WifiPhase phase_ = WifiPhase::Off;
    WifiError error_ = WifiError::None;
    Ssid current_ssid_;
    int8_t current_rssi_ = 0;
};

}  // namespace transport
