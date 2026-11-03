#pragma once

// Prawdziwe źródło snapshotów: pilot mówi kilkoma językami naraz (ADR 0006–0008).
//
// - Z906 — wprost z kontrolerem, przez `Z906Source`;
// - Apple TV, klimatyzacja i skróty kanałów TV — kontraktem v1 z Home Assistantem,
//   przez `transport::HaClient`;
// - telewizor LG — wprost z serwerem Glasshouse na TV, przez `transport::TvClient`.
//
// Na zewnątrz to jedno źródło z jedną, własną rewizją: każdy nowy fragment stanu
// daje nowy, pełny snapshot. Dzięki temu AppController nie wie, że świat ma dwie połowy.
//
// Fazę łącza wyznacza Z906 — to on zdejmuje nakładkę łączenia. Home Assistant
// i Glasshouse tylko ożywiają albo wygaszają sekcje Apple TV i telewizora, więc
// ich brak nie blokuje dźwięku.

#include "application/snapshot_source.hpp"
#include "application/z906_source.hpp"
#include "transport/ha_client.hpp"
#include "transport/mqtt_bus.hpp"
#include "transport/tv_client.hpp"

namespace application {

class LiveSource final : public SnapshotSource {
public:
    void start() override;
    void send(const domain::UiIntent& intent) override;
    void request_sync() override;
    bool poll(SourceEvent& out) override;
    const domain::ChannelList* channels() const override { return &channels_; }
    const domain::Weather* weather() const override { return &weather_; }

    void stop();
    // Dotyczą kontrolera Z906 — od niego zależy, czy pilot w ogóle działa.
    bool configured() const { return z906_.configured(); }
    transport::MqttError error() const { return z906_.error(); }
    // `true`, gdy podano identyfikator pilota i klient Home Assistanta pracuje.
    bool home_assistant() const { return ha_.running(); }
    // `true`, gdy podano prefiks Glasshouse i klient telewizora pracuje.
    bool television() const { return tv_.running(); }

private:
    void merge_into(SourceEvent& out);

    // Jedno połączenie z brokerem dla wszystkich trzech klientów.
    transport::MqttBus bus_;
    Z906Source z906_;
    transport::HaClient ha_;
    transport::TvClient tv_;

    domain::Z906State z906_state_;
    domain::AppleTvState appletv_;
    domain::ClimateState climate_;
    domain::TvState tv_state_;
    domain::ChannelList channels_;
    // Nie czyszczona przy wybudzeniu ani po utracie brokera — to treść (ADR 0011).
    domain::Weather weather_;
    // Bufor odbioru z klienta HA. Pole, a nie zmienna lokalna: poll() działa
    // na ciasnym stosie zadania LVGL.
    transport::HaEvent ha_event_;
    transport::TvEvent tv_event_;
    // Snapshot wychodzi dopiero po świeżym stanie Z906. Sam stan Apple TV czy TV nie może
    // zdjąć nakładki łączenia, gdy dźwięk jeszcze nie odpowiedział (ADR 0005).
    bool z906_fresh_ = false;
    uint32_t revision_ = 0;
};

}  // namespace application
