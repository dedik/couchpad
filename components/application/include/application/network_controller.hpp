#pragma once

// Sieć widziana od strony interfejsu: lista sieci, stan połączenia, zapamiętane
// dane dostępowe. Interfejs nie zna `transport` ani NVS — rozmawia wyłącznie tutaj.
//
// Zależności idą w dół: ui → application → transport / storage.

#include "storage/settings.hpp"
#include "transport/wifi_service.hpp"

namespace application {

class NetworkController {
public:
    using ChangeHandler = void (*)(void* context);

    explicit NetworkController(transport::WifiService& wifi) : wifi_(wifi) {}

    // Uruchamia radio i, jeśli coś zapamiętano, od razu łączy.
    void start();

    // Wołane cyklicznie z zadania UI.
    void pump();

    void scan();
    // Łączy i zapamiętuje po udanej próbie — nie wcześniej, żeby nie zapisywać
    // hasła, które okazało się złe.
    void connect(const transport::Ssid& ssid, const transport::Password& password);
    void forget();

    transport::WifiPhase phase() const { return wifi_.phase(); }
    transport::WifiError error() const { return wifi_.error(); }
    const transport::AccessPoint* access_points() const { return wifi_.access_points(); }
    std::size_t access_point_count() const { return wifi_.access_point_count(); }
    const transport::Ssid& current_ssid() const { return wifi_.current_ssid(); }
    int8_t current_rssi() const { return wifi_.current_rssi(); }
    bool scanning() const { return wifi_.phase() == transport::WifiPhase::Scanning; }
    // Sieć zapamiętana w NVS, nawet gdy w tej chwili nie ma połączenia.
    const transport::Ssid& remembered_ssid() const { return remembered_; }

    // Stan łącza do diagnostyki: przy słabym sygnale w salonie to pierwsze, co warto zobaczyć.
    uint8_t last_reason() const { return wifi_.last_reason(); }
    uint16_t drops() const { return drops_; }
    uint16_t failures() const { return failures_; }

    void set_on_change(ChangeHandler handler, void* context) {
        on_change_ = handler;
        on_change_context_ = context;
    }

private:
    transport::WifiService& wifi_;
    void retry_remembered();

    transport::Ssid remembered_;
    // Po zerwaniu wracamy na sieć sami, ale z odstępem — inaczej pilot zasypałby
    // koprocesor próbami, gdy router jest wyłączony.
    int64_t next_retry_ms_ = 0;
    transport::Password pending_password_;
    transport::Ssid pending_ssid_;
    bool awaiting_save_ = false;

    transport::WifiPhase last_phase_ = transport::WifiPhase::Off;
    uint16_t drops_ = 0;     // utraty działającego połączenia od startu
    uint16_t failures_ = 0;  // nieudane próby połączenia od startu

    ChangeHandler on_change_ = nullptr;
    void* on_change_context_ = nullptr;
};

}  // namespace application
