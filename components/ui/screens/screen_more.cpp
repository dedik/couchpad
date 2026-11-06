#include <cstdio>

#include "esp_app_desc.h"
#include "ui_internal.hpp"

namespace ui {
namespace {

// Poziom (makieta docs/ux/mockup-landscape): lista ustawień w wąskiej kolumnie, a wybrana
// podstrona zadokowana obok niej, na prawej części ekranu, zamiast zasłaniać wszystko.
constexpr int32_t kWideList = 290;
constexpr std::size_t kSettingsCount = 8;

// Podstrony w kolejności wierszy listy.
const widgets::Page* settings_page(const Context& context, std::size_t index) {
    const widgets::Page* pages[kSettingsCount] = {
        &context.wifi.page,     &context.mqtt.page,          &context.device.display,
        &context.device.sound,  &context.device.place,       &context.device.input,
        &context.device.diagnostics, &context.device.about,
    };
    return pages[index];
}

const widgets::ListRow* settings_row(const Context& context, std::size_t index) {
    const widgets::ListRow* rows[kSettingsCount] = {
        &context.more.wifi,  &context.more.mqtt,  &context.more.brightness,
        &context.more.sleep, &context.more.place, &context.more.input,
        &context.more.diagnostics, &context.more.about,
    };
    return rows[index];
}

bool page_open(const widgets::Page& page) {
    return page.root != nullptr && !lv_obj_has_flag(page.root, LV_OBJ_FLAG_HIDDEN);
}

// W poziomie otwarta jest najwyżej jedna podstrona: nowa zastępuje poprzednią.
void open_setting(Context& context, void (*open)(Context&)) {
    if (theme::landscape()) {
        close_settings_pages(context);
    }
    open(context);
}

void on_open_wifi(void* raw) { open_setting(*static_cast<Context*>(raw), &open_wifi_page); }
void on_open_mqtt(void* raw) { open_setting(*static_cast<Context*>(raw), &open_mqtt_page); }
void on_open_display(void* raw) { open_setting(*static_cast<Context*>(raw), &open_display_page); }
void on_open_sound(void* raw) { open_setting(*static_cast<Context*>(raw), &open_sound_page); }
void on_open_about(void* raw) { open_setting(*static_cast<Context*>(raw), &open_about_page); }
void on_open_place(void* raw) { open_setting(*static_cast<Context*>(raw), &open_place_page); }
void on_open_input(void* raw) { open_setting(*static_cast<Context*>(raw), &open_input_page); }
void on_open_diagnostics(void* raw) {
    open_setting(*static_cast<Context*>(raw), &open_diagnostics_page);
}
}  // namespace

void close_settings_pages(Context& context) {
    for (std::size_t i = 0; i < kSettingsCount; ++i) {
        widgets::show_page(*settings_page(context, i), false);
    }
    context.wifi.open = false;
}

void dock_settings_pages(Context& context) {
    if (!theme::landscape()) {
        return;
    }
    const int32_t left = theme::kRail + theme::kGutterWide + kWideList + theme::kS4;
    const int32_t width = lv_display_get_horizontal_resolution(lv_display_get_default()) - left;
    for (std::size_t i = 0; i < kSettingsCount; ++i) {
        lv_obj_t* root = settings_page(context, i)->root;
        lv_obj_set_size(root, width, LV_PCT(100));
        lv_obj_align(root, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_border_width(root, 1, LV_PART_MAIN);
        lv_obj_set_style_border_side(root, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_color(root, theme::line(), LV_PART_MAIN);
    }
}

void sync_settings_detail(Context& context) {
    if (!theme::landscape()) {
        return;
    }
    if (context.screen != ScreenId::More) {
        // Zadokowana podstrona należy do ustawień: nie może zostać nad TV czy Z906.
        close_settings_pages(context);
        return;
    }
    for (std::size_t i = 0; i < kSettingsCount; ++i) {
        if (page_open(*settings_page(context, i))) {
            return;
        }
    }
    // Pusta prawa połowa wyglądała na błąd: po wejściu w ustawienia od razu widać Ekran.
    open_display_page(context);
}

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

    if (theme::landscape()) {
        // Wąska kolumna: bez opisów pod nazwą. Długie podsumowania (Ekran, wersja)
        // znikają — te same wartości widać obok, w otwartej podstronie.
        lv_obj_set_width(screen.section, kWideList);
        for (std::size_t i = 0; i < kSettingsCount; ++i) {
            lv_obj_add_flag(settings_row(context, i)->note, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(screen.brightness.value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(screen.about.value, LV_OBJ_FLAG_HIDDEN);
    }

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

    // Poziom: wiersz otwartej podstrony obwiedziony akcentem, jak wybór na liście.
    if (theme::landscape()) {
        for (std::size_t i = 0; i < kSettingsCount; ++i) {
            const bool open = page_open(*settings_page(context, i));
            lv_obj_set_style_border_color(settings_row(context, i)->root,
                                          open ? theme::accent() : theme::line(), LV_PART_MAIN);
        }
    }

}

}  // namespace ui
