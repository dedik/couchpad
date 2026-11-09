#pragma once

// Kontrakt MQTT v1 po stronie pilota, w zakresie Apple TV (ADR 0007) i klimatyzacji
// (ADR 0009): snapshot z tematu `state` i polecenia `appletv.*`, `climate.*` na temat
// `command`. Oraz `tv.channel.open` — skrót kanału, który bridge wykonuje po HTTP na
// Glasshouse (ADR 0008).
// Format opisuje docs/architecture/mqtt-contract.md i to on, a nie ten plik,
// jest źródłem prawdy — zmiana tematu lub payloadu wymaga nowej wersji.
//
// Z906 idzie wprost do kontrolera (ADR 0006), więc sekcję `z906` z tego tematu
// pomijamy. Klient pracuje we własnym zadaniu esp-mqtt i oddaje wyniki kolejką;
// zadanie UI nigdy nie czeka na sieć.

#include <atomic>
#include <cstdint>

#include "domain/channels.hpp"
#include "domain/intent.hpp"
#include "domain/snapshot.hpp"
#include "domain/weather.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "storage/settings.hpp"
#include "transport/mqtt_bus.hpp"

namespace transport {

struct HaEvent {
    enum class Kind : uint8_t {
        Snapshot,   // nowy obraz sekcji `appletv` i `climate`
        Offline,    // brak połączenia z brokerem: stan Apple TV i klimy przestaje być świeży
    };
    Kind kind = Kind::Offline;
    domain::AppleTvState appletv;
    domain::ClimateState climate;
    domain::LightsState lights;
    domain::RadiatorsState radiators;
};

class HaClient {
public:
    // `false`, gdy nie podano identyfikatora pilota — wtedy Apple TV zostaje
    // niedostępne, a reszta pilota pracuje bez Home Assistanta.
    // Podpina się pod wspólne połączenie przed `bus.start()`.
    bool start(MqttBus& bus, const storage::MqttSettings& settings);
    void stop();
    bool running() const { return handle_ != nullptr; }

    // Nie blokują wołającego. Zamiary spoza kontraktu są pomijane.
    void publish_intent(const domain::UiIntent& intent);
    void request_sync();

    // Odbiór wyłącznie z zadania UI.
    bool poll(HaEvent& out);
    // Kopiuje nową listę kanałów, jeśli przyszła od ostatniego wywołania.
    bool take_channels(domain::ChannelList& out);
    // Kopiuje nową pogodę (ADR 0011), jeśli przyszła od ostatniego wywołania.
    bool take_weather(domain::Weather& out);

private:
    static void on_mqtt_event(void* self, const char* base, int32_t id, void* data);

    void emit(const HaEvent& event);
    void handle_payload(const char* data, int length);
    void handle_channels(const char* data, int length);
    void handle_weather(const char* data, int length);
    void publish_command(const char* action, const char* args_json, int qos = 1);
    void publish_raw(const char* topic, const char* payload, int qos, bool retain);

    void* handle_ = nullptr;  // esp_mqtt_client_handle_t
    MqttBus* bus_ = nullptr;
    QueueHandle_t events_ = nullptr;
    // Polecenie wysłane bez połączenia przepada: odtworzone po powrocie łącza
    // kroki trackpada poruszyłyby Apple TV bez udziału użytkownika.
    std::atomic<bool> connected_{false};

    storage::MqttSettings settings_;
    core::Text<96> topic_state_;
    core::Text<96> topic_command_;
    core::Text<96> topic_availability_;
    core::Text<96> topic_channels_;
    core::Text<96> topic_weather_;

    // Zdarzenie składane w zadaniu MQTT. Pole, a nie zmienna lokalna: z klimą ma
    // ok. 1,7 kB, a stos zadania MQTT — 6 kB, z czego część zjada cJSON.
    HaEvent outgoing_;

    // Rewizję nadaje Home Assistant. Wiadomość retained odsyłana po ponownej
    // subskrypcji ma tę samą rewizję i nie może udawać świeżego stanu.
    uint32_t last_revision_ = 0;

    // Snapshot bywa dłuższy niż jedna ramka, więc sklejamy go z fragmentów.
    char* assembly_ = nullptr;
    int assembly_len_ = 0;
    int assembly_total_ = 0;
    core::Text<96> assembly_topic_;  // temat niesie wyłącznie pierwszy kawałek
    // Połączenie jest wspólne, więc kawałki cudzych wiadomości też tu trafiają.
    bool assembly_mine_ = false;

    // Lista kanałów: składana w zadaniu MQTT wprost pod muteksem, oddawana
    // zadaniu UI kopią. Jeden bufor tutaj i jeden w źródle snapshotów.
    domain::ChannelList channels_ready_;
    bool channels_fresh_ = false;
    // Pogoda pod tym samym muteksem co lista kanałów: mała, rzadka, oddawana kopią.
    domain::Weather weather_ready_;
    bool weather_fresh_ = false;
    SemaphoreHandle_t channels_lock_ = nullptr;
};

}  // namespace transport
