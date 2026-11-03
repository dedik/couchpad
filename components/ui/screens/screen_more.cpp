#include <cstdio>

#include "esp_app_desc.h"
#include "ui_internal.hpp"

namespace ui {
namespace {

void on_open_wifi(void* raw) { open_wifi_page(*static_cast<Context*>(raw)); }
void on_open_mqtt(void* raw) { open_mqtt_page(*static_cast<Context*>(raw)); }
void on_open_display(void* raw) { open_display_page(*static_cast<Context*>(raw)); }
void on_open_sound(void* raw) { open_sound_page(*static_cast<Context*>(raw)); }
void on_open_about(void* raw) { open_about_page(*static_cast<Context*>(raw)); }
void on_open_place(void* raw) { open_place_page(*static_cast<Context*>(raw)); }
void on_open_input(void* raw) { open_input_page(*static_cast<Context*>(raw)); }
void on_open_diagnostics(void* raw) { open_diagnostics_page(*static_cast<Context*>(raw)); }
}  // namespace

void build_more(Context& context, lv_obj_t* parent) {
    MoreScreen& screen = context.more;
    screen.section = widgets::make_section(parent);
    lv_obj_set_style_pad_row(screen.section, theme::kS2, LV_PART_MAIN);
    // Siedem wierszy mieści się co do kilku pikseli; każdy kolejny wyszedłby
    // poza ekran, więc lista przewija się w pionie.
    lv_obj_add_flag(screen.section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(screen.section, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(screen.section, LV_SCROLLBAR_MODE_ACTIVE);

    screen.wifi = widgets::make_list_row(screen.section, LV_SYMBOL_WIFI, "Wi-Fi", "", "",
                                         &on_open_wifi, &context);
    screen.mqtt = widgets::make_list_row(screen.section, LV_SYMBOL_LOOP, "MQTT",
                                         "broker i kontroler Z906", "", &on_open_mqtt, &context);
    screen.brightness = widgets::make_list_row(screen.section, LV_SYMBOL_EYE_OPEN, "Ekran",
                                               "styl, jasność, wygaszanie", "", &on_open_display,
                                               &context);
    screen.sleep = widgets::make_list_row(screen.section, LV_SYMBOL_VOLUME_MAX, "Dźwięk",
                                          "limit głośności Z906", "", &on_open_sound, &context);
    screen.place = widgets::make_list_row(screen.section, LV_SYMBOL_GPS, "Pokój i czas",
                                          "nazwa pokoju, strefa czasowa", "", &on_open_place,
                                          &context);
    screen.input = widgets::make_list_row(screen.section, LV_SYMBOL_KEYBOARD,
                                          "Klawiatura i gładzik", "styl, tekst na żywo, czułość",
                                          "", &on_open_input, &context);
    screen.diagnostics = widgets::make_list_row(screen.section, LV_SYMBOL_LIST, "Diagnostyka",
                                                "IP, łącze z domem, stan urządzeń", "",
                                                &on_open_diagnostics, &context);
    screen.about = widgets::make_list_row(screen.section, LV_SYMBOL_FILE, "O urządzeniu",
                                          "wersja, restart, ustawienia fabryczne", "",
                                          &on_open_about, &context);

}

void render_more(Context& context) {
    MoreScreen& screen = context.more;

    // Wiersz Wi-Fi pokazuje stan bez wchodzenia w podstronę.
    const auto phase = context.network->phase();
    const bool connected = phase == transport::WifiPhase::Connected;
    lv_label_set_text(screen.wifi.note,
                      connected ? context.network->current_ssid().c_str()
                      : phase == transport::WifiPhase::Connecting ? "łączenie…"
                                                                  : "brak połączenia");
    lv_label_set_text(screen.wifi.value, connected ? "połączono" : "");

    const auto link = context.controller->state().phase();
    lv_label_set_text(screen.mqtt.note, "multimedia_remote/v1");
    lv_label_set_text(screen.mqtt.value, link == domain::LinkPhase::Ready ? "połączono" : "brak");
    lv_obj_set_style_text_color(screen.mqtt.value,
                                link == domain::LinkPhase::Ready ? theme::ok() : theme::dim(),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(screen.wifi.value, connected ? theme::ok() : theme::dim(),
                                LV_PART_MAIN);

    char value[40];
    // Styl bez godzin: „auto” mieści się obok podpisu, okno widać na podstronie.
    const char* const style_names[] = {"ciemny", "jasny", "auto"};
    const uint8_t style = storage::get(storage::Pref::ThemeMode);
    const char* style_name = style_names[style < 3 ? style : 0];
    const unsigned bright = storage::get(storage::Pref::Brightness);
    const uint32_t timeout = screen_timeout_ms();
    if (timeout == 0) {
        std::snprintf(value, sizeof(value), "%s · %u%% · zawsze", style_name, bright);
    } else if (timeout < 60000) {
        std::snprintf(value, sizeof(value), "%s · %u%% · %u s", style_name, bright,
                      static_cast<unsigned>(timeout / 1000));
    } else {
        std::snprintf(value, sizeof(value), "%s · %u%% · %u min", style_name, bright,
                      static_cast<unsigned>(timeout / 60000));
    }
    lv_label_set_text(screen.brightness.value, value);
    const uint8_t limit = volume_limit();
    if (limit >= 100) {
        lv_label_set_text(screen.sleep.value, "bez limitu");
    } else {
        std::snprintf(value, sizeof(value), "maks. %u%%", static_cast<unsigned>(limit));
        lv_label_set_text(screen.sleep.value, value);
    }
    lv_label_set_text(screen.about.value, esp_app_get_description()->version);
    lv_label_set_text(screen.place.value, context.room.c_str());

}

}  // namespace ui
