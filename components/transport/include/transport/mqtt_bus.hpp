#pragma once

// Jedno połączenie z brokerem dla wszystkich klientów pilota: Z906 (ADR 0006),
// Home Assistanta (ADR 0007, 0009) i Glasshouse (ADR 0008). Wszyscy rozmawiają z tym
// samym brokerem, a każdy klient esp-mqtt to osobne zadanie z 6 kB stosu, dwa bufory
// i gniazdo TCP w pamięci wewnętrznej. Klienci podpinają się handlerami zdarzeń
// (esp-mqtt pozwala na kilka) i sami odsiewają swoje tematy.
//
// Kolejność: `open()`, `attach()` każdego klienta, `start()`. Podpięcie po starcie
// przegapiłoby zdarzenie połączenia, w którym klienci subskrybują swoje tematy.
//
// Wysyłka idzie własnym zadaniem przez `esp_mqtt_client_publish`, a nie kolejką
// biblioteki (`enqueue`). Kolejkę biblioteki zadanie esp-mqtt opróżnia raz na obieg
// pętli, a obieg trwa tyle, co limit odczytu (`MQTT_POLL_READ_TIMEOUT_MS`). Krótki
// limit dawał szybkie klawisze, ale zrywał połączenie przy każdej przerwie dłuższej
// niż on w środku dużej wiadomości (lista kanałów, telemetria TV) — na wspólnym
// połączeniu co półtorej sekundy. Własne zadanie wysyła od razu, więc limit może
// zostać domyślny.

#include <cstdint>

#include "core/text.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "storage/settings.hpp"

namespace transport {

class MqttBus {
public:
    using Handler = void (*)(void* arg, const char* base, int32_t id, void* data);

    // Tworzy klienta bez łączenia. `false`, gdy brakuje adresu brokera.
    bool open(const storage::MqttSettings& settings);
    void attach(Handler handler, void* arg);
    // Bez adresu IP klient nie łączy się od razu, tylko czeka na IP_EVENT_STA_GOT_IP.
    // Inaczej esp-mqtt próbuje kilka razy z „Host is unreachable”, a każda nieudana
    // próba to dla pilota błąd łącza na ekranie, choć Wi-Fi po prostu jeszcze wstaje.
    bool start();
    // Zatrzymuje i niszczy klienta. Klienci muszą wcześniej zapomnieć uchwyt.
    void close();

    void* handle() const { return handle_; }  // esp_mqtt_client_handle_t

    // Nie blokuje wołającego (zadanie UI): odkłada wiadomość dla zadania wysyłki.
    // Przy pełnej kolejce wiadomość przepada — polecenie bez potwierdzenia skończy
    // się na ekranie brakiem potwierdzenia, a nie zamrożonym dotykiem.
    void publish(const char* topic, const char* payload, int qos, bool retain);

    static constexpr std::size_t kMaxTopic = 112;
    static constexpr std::size_t kMaxPayload = 384;

private:
    struct Outgoing {
        char topic[kMaxTopic];
        char payload[kMaxPayload];
        uint8_t qos;
        bool retain;
    };

    static void sender_entry(void* self);
    void sender();
    static void on_got_ip(void* self, const char* base, int32_t id, void* data);
    bool start_client();
    void cancel_ip_wait();

    void* handle_ = nullptr;
    void* ip_waiter_ = nullptr;  // esp_event_handler_instance_t, gdy start czeka na IP
    QueueHandle_t outgoing_ = nullptr;
    TaskHandle_t sender_task_ = nullptr;
    // Zadanie wysyłki trzyma ją na czas `publish`, `close()` — na czas niszczenia klienta.
    SemaphoreHandle_t lock_ = nullptr;
    Outgoing sending_{};
    storage::MqttSettings settings_;
    core::Text<48> client_id_;
    core::Text<96> will_topic_;
};

}  // namespace transport
