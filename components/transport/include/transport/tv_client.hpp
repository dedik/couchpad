#pragma once

// Bezpośrednia rozmowa z telewizorem LG przez serwer Glasshouse (ADR 0008).
//
// Glasshouse działa na samym telewizorze i publikuje stan w tematach
// `<prefiks>/status`, `<prefiks>/telemetry` oraz natychmiastowych `<prefiks>/state/...`;
// polecenia przyjmuje na `<prefiks>/command/<nazwa>`. Kanałów po MQTT nie zna —
// skróty kanałów idą kontraktem v1 przez Home Assistanta (`HaClient`).
//
// Obraz stanu składamy tutaj z kilku tematów. Klient pracuje we własnym zadaniu
// esp-mqtt; zadanie UI nigdy nie czeka na sieć.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "domain/intent.hpp"
#include "domain/snapshot.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "storage/settings.hpp"
#include "transport/mqtt_bus.hpp"

namespace transport {

struct TvEvent {
    enum class Kind : uint8_t {
        Snapshot,   // nowy obraz telewizora
        Offline,    // brak brokera: stan telewizora przestaje być świeży
    };
    Kind kind = Kind::Offline;
    domain::TvState tv;
};

class TvClient {
public:
    // `false`, gdy prefiks jest pusty — telewizor zostaje wtedy niedostępny.
    // Podpina się pod wspólne połączenie przed `bus.start()`.
    bool start(MqttBus& bus, const storage::MqttSettings& settings);
    void stop();
    bool running() const { return handle_ != nullptr; }

    // Nie blokują wołającego. Zamiary spoza telewizora są pomijane.
    void publish_intent(const domain::UiIntent& intent);
    void request_sync();

    // Odbiór wyłącznie z zadania UI.
    bool poll(TvEvent& out);

private:
    // Opcja z identyfikatorem, którego oczekuje polecenie, i nazwą do pokazania.
    struct Option {
        domain::Label id;
        domain::Label name;
    };
    using Options = std::array<Option, domain::kMaxOptions>;

    static void on_mqtt_event(void* self, const char* base, int32_t id, void* data);
    static void on_offline_timer(void* self);

    void handle_message(const char* topic, int topic_len, const char* data, int data_len);
    void apply_telemetry(const char* data, int length);
    void rebuild();
    void emit(const TvEvent& event);
    void publish_command(const char* name, const char* value, int qos);
    void subscribe_all();

    void* handle_ = nullptr;  // esp_mqtt_client_handle_t
    MqttBus* bus_ = nullptr;
    QueueHandle_t events_ = nullptr;
    void* offline_timer_ = nullptr;  // esp_timer_handle_t
    std::atomic<bool> connected_{false};

    storage::MqttSettings settings_;
    core::Text<48> prefix_;          // `<prefiks>/`

    // Surowe kawałki stanu, każdy z własnego tematu. Pisze je wyłącznie zadanie MQTT.
    core::Text<16> status_;          // online / off / asleep / offline
    domain::Label app_;              // krótki identyfikator aplikacji: hdmi4, livetv
    domain::Label picture_raw_;      // identyfikator trybu obrazu
    bool live_app_ = false;          // app_ przyszedł z tematu natychmiastowego
    bool live_picture_ = false;
    int8_t system_on_ = -1;          // -1 nieznane, 0 wyłączony, 1 włączony
    Options inputs_{};
    std::size_t input_count_ = 0;
    Options modes_{};
    std::size_t mode_count_ = 0;

    domain::TvState last_;
    bool emitted_ = false;
    // Po żądaniu odświeżenia następny obraz wychodzi, nawet gdy nic się nie zmieniło.
    std::atomic<bool> resync_{false};

    // Telemetria ma kilka kilobajtów i przychodzi w kawałkach. Temat niesie
    // wyłącznie pierwszy, więc zapamiętujemy, czego dotyczy reszta.
    char* assembly_ = nullptr;
    int assembly_len_ = 0;
    int assembly_total_ = 0;
    core::Text<96> assembly_topic_;
    // Połączenie jest wspólne, więc kawałki cudzych wiadomości też tu trafiają.
    bool assembly_mine_ = false;
};

}  // namespace transport
