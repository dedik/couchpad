#pragma once

// Bezpośrednia rozmowa z kontrolerem Logitech Z906 po MQTT (ADR 0006).
//
// Kontroler publikuje stan w osobnych tematach, po jednej wartości w każdym,
// więc pełny obraz składamy tutaj. Rewizji nie podaje — numerujemy ją sami,
// bo interfejs ignoruje snapshot nie nowszy od ostatniego.

#include <atomic>
#include <cstdint>

#include "domain/app_state.hpp"
#include "domain/intent.hpp"
#include "domain/snapshot.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "storage/settings.hpp"
#include "transport/mqtt_bus.hpp"
#include "transport/mqtt_error.hpp"  // MqttError

namespace transport {

// Zdarzenie niesie wyłącznie stan Z906, nie cały snapshot urządzeń: powstaje na
// stosie zadania MQTT (6 kB), a kolejka ma dwanaście miejsc w pamięci wewnętrznej.
// Pełny snapshot, z klimatyzacją, przepełniał ten stos i zjadał ok. 43 kB RAM-u.
struct Z906Event {
    enum class Kind : uint8_t { Snapshot, Phase };
    Kind kind = Kind::Phase;
    uint32_t revision = 0;
    domain::Z906State z906;
    domain::LinkPhase phase = domain::LinkPhase::Connecting;
    domain::LinkStep step = domain::LinkStep::Mqtt;
};

class Z906Client {
public:
    // Podpina się pod wspólne połączenie przed `bus.start()`. `false`, gdy
    // ustawienia są niekompletne.
    bool start(MqttBus& bus, const storage::MqttSettings& settings);
    // Zapomina połączenie; samo połączenie zamyka właściciel `MqttBus`.
    void stop();

    void publish_intent(const domain::UiIntent& intent);
    void request_sync();

    bool poll(Z906Event& out);
    MqttError error() const { return error_.load(); }

private:
    static void on_mqtt_event(void* self, const char* base, int32_t id, void* data);

    void apply_topic(const char* topic, int topic_len, const char* data, int data_len);
    void emit_snapshot();
    void emit_phase(domain::LinkPhase phase, domain::LinkStep step);
    void publish_value(const char* leaf, const char* value);

    void* handle_ = nullptr;
    MqttBus* bus_ = nullptr;
    QueueHandle_t events_ = nullptr;

    storage::MqttSettings settings_;
    core::Text<96> topic_state_prefix_;   // z906/<id>/state/
    core::Text<96> topic_command_prefix_; // z906/<id>/command/
    core::Text<96> topic_availability_;

    // Obraz składany z pojedynczych tematów.
    domain::Z906State state_;
    uint32_t revision_ = 0;
    bool available_ = false;
    bool ready_ = false;
    // Po żądaniu odświeżenia pierwszy odebrany temat musi dać snapshot, nawet
    // gdy nic się nie zmieniło: to on zdejmuje nakładkę łączenia.
    bool resync_ = false;
    // Zapisywane z zadania MQTT, czytane z zadania UI.
    std::atomic<bool> connected_{false};
    std::atomic<MqttError> error_{MqttError::None};
};

}  // namespace transport
