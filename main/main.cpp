// Composition root: składa warstwy i oddaje sterowanie zadaniu UI.
//
// Źródło snapshotów podmienia się w tych kilku liniach — `ui` i `domain` nie
// wiedzą, co stoi po drugiej stronie interfejsu SnapshotSource.

#include "application/app_controller.hpp"
#include "application/live_source.hpp"
#include "application/network_controller.hpp"
#include "board/board.hpp"
#include "core/memory.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage/settings.hpp"
#include "transport/log_sink.hpp"
#include "transport/wifi_service.hpp"
#include "ui/ui.hpp"

namespace {

constexpr char kTag[] = "pilot";

// Pilot rozmawia wprost z kontrolerem Z906 (ADR 0006), z telewizorem przez
// Glasshouse (ADR 0008), a z Apple TV i klimą przez Home Assistanta (ADR 0007,
// 0009); wszystko składa LiveSource.
//
// Obiekty powstają w PSRAM na początku app_main: same LiveSource i AppController
// trzymają ok. 30 kB stanu (dwie listy kanałów, kopie snapshotu), które w .bss
// zabierałyby pamięć wewnętrzną.
application::LiveSource* g_live = nullptr;
application::AppController* g_controller = nullptr;
transport::WifiService* g_wifi = nullptr;
application::NetworkController* g_network = nullptr;

}  // namespace

extern "C" void app_main(void) {
#if CONFIG_PILOT_REMOTE_LOG
    // Od pierwszej linii: start urządzenia też ma trafić do logu przez MQTT.
    transport::log_sink::capture();
#endif
    // ESP-Hosted wypisuje nazwę sieci przy każdym ustawieniu Wi-Fi na poziomie INFO.
    // Nazwa sieci jest prywatna (AGENTS.md), więc z tego tagu zostają tylko ostrzeżenia.
    esp_log_level_set("eh_wifi", ESP_LOG_WARN);
    core::memory::log_stage("start");
    g_live = &core::memory::make_in_psram<application::LiveSource>();
    g_controller = &core::memory::make_in_psram<application::AppController>(*g_live);
    g_wifi = &core::memory::make_in_psram<transport::WifiService>();
    g_network = &core::memory::make_in_psram<application::NetworkController>(*g_wifi);
    // Ustawienia przed ekranem: orientację ustala się przy jego starcie.
    storage::init();
    if (!board::start(board::rotation_from(storage::get(storage::Pref::Orientation)))) {
        ESP_LOGE(kTag, "ekran nie ruszyl — nie ma czego pokazywac");
        return;
    }
    core::memory::log_stage("ekran");
#if CONFIG_PILOT_USB_CONSOLE
    board::start_usb_console();
#endif
    // Jasność z ustawień (domyślnie 100 %), także po każdym wybudzeniu.
    board::set_brightness(storage::get(storage::Pref::Brightness));

    {
        // Wszystkie wywołania LVGL wykonuje zadanie UI (ADR 0003); budowa drzewa
        // dzieje się pod jego blokadą.
        board::Lock lock(0);
        ui::Services services;
        services.controller = g_controller;
        services.network = g_network;
        services.live = g_live;
        services.display_power = &board::set_display_on;
        services.display_brightness = &board::set_brightness;
        services.display_dim = &board::dim;
        ui::start(services);
    }

    g_network->start();
#if CONFIG_PILOT_REMOTE_LOG
    // Dopiero po starcie sieci: klient MQTT potrzebuje stosu TCP/IP, który
    // podnosi usługa Wi-Fi. Wcześniejsze linie czekają w buforze.
    {
        storage::MqttSettings mqtt;
        if (storage::load_mqtt(mqtt)) {
            const char* id = !mqtt.remote_id.empty() ? mqtt.remote_id.c_str() : "pilot";
            transport::log_sink::start(mqtt, id);
        }
    }
#endif
    g_controller->start();
    core::memory::log_stage("sieć i klienci");
    // LVGL alokuje ze sterty ESP-IDF, więc warto widzieć, ile zostało po zbudowaniu
    // wszystkich pięciu ekranów. Wbudowana pula LVGL 64 kB na to nie starczała.
    ESP_LOGI(kTag, "interfejs gotowy");
    // Drugi pomiar po połączeniu: klienci MQTT, Wi-Fi i pierwszy stan już mają swoje bufory,
    // a zapas stosów pokazuje, ile każde zadanie naprawdę zużyło.
    static esp_timer_handle_t report = nullptr;
    const esp_timer_create_args_t args = {
        .callback = [](void*) {
            core::memory::log_stage("po 30 s");
            core::memory::log_tasks();
        },
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mem_report",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &report) == ESP_OK) {
        esp_timer_start_once(report, 30 * 1000 * 1000);
    }
}
