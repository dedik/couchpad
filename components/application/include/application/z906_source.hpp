#pragma once

// Kontroler Z906 jako część LiveSource. Cienka przejściówka: cała rozmowa po
// MQTT siedzi w `transport`, tutaj zostaje tłumaczenie zdarzeń na SourceEvent.

#include "application/snapshot_source.hpp"
#include "transport/z906_client.hpp"

namespace application {

class Z906Source final {
public:
    // Podpina klienta pod wspólne połączenie (przed `bus.start()`).
    void start(transport::MqttBus& bus, const storage::MqttSettings& settings, bool loaded);
    // Połączenie nie wystartowało: ekran ma pokazać błąd, a nie wieczne łączenie.
    void report_failure() { report_missing_settings_ = true; }
    void send(const domain::UiIntent& intent);
    void request_sync();
    bool poll(SourceEvent& out);

    void stop();
    // `false`, gdy nie wpisano jeszcze adresu brokera ani identyfikatora kontrolera.
    bool configured() const { return configured_; }
    transport::MqttError error() const { return client_.error(); }

private:
    transport::Z906Client client_;
    // Pole, nie zmienna lokalna: zdarzenie niesie cały snapshot, a `poll()`
    // biegnie na stosie zadania LVGL.
    transport::Z906Event event_;
    bool configured_ = false;
    bool report_missing_settings_ = false;
};

}  // namespace application
