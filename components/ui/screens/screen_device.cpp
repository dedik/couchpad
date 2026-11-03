// Podstrony ustawień samego pilota: Ekran, Dźwięk, O urządzeniu.
//
// Wartości domyślne odtwarzają zachowanie sprzed tych ustawień (jasność 100 %,
// wygaszanie po 30 s, bez przyciemniania, bez limitu głośności), więc pilot,
// którego nikt nie skonfigurował, działa dokładnie jak dotąd.

#include <cstdio>
#include <ctime>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "transport/time_sync.hpp"
#include "ui_internal.hpp"

namespace ui {
namespace {

// ---------- Ekran ----------

constexpr uint8_t kBrightness[] = {20, 40, 60, 80, 100};
const char* const kBrightnessLabels[] = {"20%", "40%", "60%", "80%", "100%"};

// Indeks zapisany w NVS (Pref::ScreenTimeout) wskazuje pozycję tej tabeli.
constexpr uint32_t kTimeoutMs[] = {15000, 30000, 60000, 120000, 0};
const char* const kTimeoutLabels[] = {"15 s", "30 s", "1 min", "2 min", "nigdy"};

const char* const kDimLabels[] = {"wyłączone", "10 s przed"};

// Tryb nocny: godziny od–do (koniec wyłącznie), w nocy jasność najwyżej kNightBrightness.
struct NightWindow {
    int from;
    int to;
};
constexpr NightWindow kNightWindows[] = {{0, 0}, {21, 7}, {22, 7}, {23, 6}};
const char* const kNightLabels[] = {"wył.", "21–7", "22–7", "23–6"};
constexpr uint8_t kNightBrightness = 20;

// ---------- Pokój i czas ----------

struct TimeZone {
    const char* label;
    const char* posix;
};
constexpr TimeZone kTimeZones[] = {
    {"Polska", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"UTC", "UTC0"},
    {"Londyn", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Kijów", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
};
const char* const kTimeZoneLabels[] = {"Polska", "UTC", "Londyn", "Kijów"};

// ---------- Klawiatura i gładzik ----------

const char* const kStyleLabels[] = {"Apple TV", "QWERTY"};
const char* const kLiveLabels[] = {"na żywo", "po zapisie"};
const char* const kAutoLabels[] = {"sama", "przyciskiem"};
// Piksele ruchu na krok: mniej = czulszy gładzik. 26 to wartość sprzed ustawień.
constexpr uint8_t kTrackpadSteps[] = {20, 26, 34};
const char* const kTrackpadLabels[] = {"czuły", "zwykły", "spokojny"};

// ---------- Dźwięk ----------

constexpr uint8_t kVolumeLimits[] = {50, 60, 70, 80, 100};
const char* const kVolumeLimitLabels[] = {"50%", "60%", "70%", "80%", "bez"};

template <std::size_t N>
std::size_t index_of(const uint8_t (&table)[N], uint8_t value, std::size_t fallback) {
    for (std::size_t i = 0; i < N; ++i) {
        if (table[i] == value) {
            return i;
        }
    }
    return fallback;
}

void on_back_display(void* raw) {
    widgets::show_page(static_cast<Context*>(raw)->device.display, false);
}
void on_back_sound(void* raw) {
    widgets::show_page(static_cast<Context*>(raw)->device.sound, false);
}
void on_back_about(void* raw) {
    widgets::show_page(static_cast<Context*>(raw)->device.about, false);
}

void on_brightness(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= sizeof(kBrightness)) {
        return;
    }
    storage::set(storage::Pref::Brightness, kBrightness[index]);
    // W nocy tryb nocny może obniżać wybraną wartość — liczymy ją razem.
    const uint8_t target = effective_brightness();
    if (context.display_brightness != nullptr) {
        context.display_brightness(target);
    }
    context.applied_brightness = target;
    render(context);
}

void on_timeout(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= sizeof(kTimeoutMs) / sizeof(kTimeoutMs[0])) {
        return;
    }
    storage::set(storage::Pref::ScreenTimeout, static_cast<uint8_t>(index));
    render(context);
}

void on_dim(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    storage::set_enabled(storage::Pref::DimBeforeSleep, index == 1);
    render(context);
}

void on_volume_limit(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= sizeof(kVolumeLimits)) {
        return;
    }
    storage::set(storage::Pref::VolumeLimit, kVolumeLimits[index]);
    render(context);
}

void on_back_place(void* raw) {
    widgets::show_page(static_cast<Context*>(raw)->device.place, false);
}
void on_back_input(void* raw) {
    widgets::show_page(static_cast<Context*>(raw)->device.input, false);
}
void on_back_diagnostics(void* raw) {
    widgets::show_page(static_cast<Context*>(raw)->device.diagnostics, false);
}

void on_night(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= sizeof(kNightWindows) / sizeof(kNightWindows[0])) {
        return;
    }
    storage::set(storage::Pref::NightMode, static_cast<uint8_t>(index));
    context.applied_brightness = -1;  // tick ustawi nową jasność od razu
    render(context);
}

void on_room_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.room.assign(value);
    if (context.room.empty()) {
        context.room.assign("SALON");
    }
    storage::save_room(context.room);
    render(context);
}

void on_room(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "POKÓJ", "Nazwa w nagłówku ekranów",
                              context.room.c_str(), widgets::EditorMode::Text, &on_room_ready,
                              &context);
}

void on_timezone(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= sizeof(kTimeZones) / sizeof(kTimeZones[0])) {
        return;
    }
    storage::set(storage::Pref::TimeZone, static_cast<uint8_t>(index));
    apply_timezone();
    context.applied_brightness = -1;  // godzina mogła przeskoczyć przez granicę nocy
    render(context);
}

void on_keyboard_style(void* raw, std::size_t index) {
    storage::set_enabled(storage::Pref::KeyboardQwerty, index == 1);
    render(*static_cast<Context*>(raw));
}

void on_keyboard_live(void* raw, std::size_t index) {
    storage::set_enabled(storage::Pref::KeyboardLive, index == 0);
    render(*static_cast<Context*>(raw));
}

void on_keyboard_auto(void* raw, std::size_t index) {
    storage::set_enabled(storage::Pref::KeyboardAutoOpen, index == 0);
    render(*static_cast<Context*>(raw));
}

void on_trackpad(void* raw, std::size_t index) {
    if (index >= sizeof(kTrackpadSteps)) {
        return;
    }
    storage::set(storage::Pref::TrackpadStep, kTrackpadSteps[index]);
    render(*static_cast<Context*>(raw));
}

const char* link_phase_text(domain::LinkPhase phase) {
    switch (phase) {
        case domain::LinkPhase::Ready: return "gotowe";
        case domain::LinkPhase::Connecting: return "łączenie";
        case domain::LinkPhase::Error: return "błąd";
    }
    return "";
}

void render_diagnostics(Context& context) {
    DeviceScreens& screen = context.device;
    char text[64];

    esp_netif_ip_info_t ip{};
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != nullptr && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        std::snprintf(text, sizeof(text), IPSTR, IP2STR(&ip.ip));
        screen.diag_ip.set(text, true);
    } else {
        screen.diag_ip.set("brak", false);
    }

    const domain::AppState& state = context.controller->state();
    screen.diag_link.set(link_phase_text(state.phase()), state.phase() == domain::LinkPhase::Ready);

    const bool z906 = state.phase() == domain::LinkPhase::Ready && state.snapshot().z906.available;
    screen.diag_z906.set(z906 ? "stan świeży" : "brak świeżego stanu", z906);

    if (!context.live->home_assistant()) {
        screen.diag_ha.set("brak identyfikatora pilota", false);
    } else {
        screen.diag_ha.set("klient pracuje", true);
    }

    const bool appletv = state.appletv_fresh();
    std::snprintf(text, sizeof(text), "%s · aplikacji %u",
                  appletv ? "dostępne" : "niedostępne",
                  static_cast<unsigned>(state.snapshot().appletv.app_count));
    screen.diag_appletv.set(text, appletv);

    std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(state.snapshot().revision));
    screen.diag_revision.set(text, true);
}

// ---------- O urządzeniu ----------

void restart_now(void*) {
    esp_restart();
}

void factory_reset_now(void*) {
    storage::factory_reset();
    esp_restart();
}

void on_restart(void* raw) {
    widgets::ask_confirm("Uruchomić ponownie?",
                         "Pilot zrestartuje się i połączy z domem od nowa. Ustawienia zostają.",
                         "Uruchom", &restart_now, raw);
}

void on_factory_reset(void* raw) {
    widgets::ask_confirm("Przywrócić ustawienia fabryczne?",
                         "Pilot zapomni sieć Wi-Fi, broker MQTT i wszystkie ustawienia, "
                         "po czym uruchomi się ponownie. Trzeba będzie wpisać je od nowa.",
                         "Przywróć", &factory_reset_now, raw);
}

lv_obj_t* add_section(lv_obj_t* body, const char* caption) {
    lv_obj_t* cap = widgets::make_cap(body, caption);
    lv_obj_set_style_margin_top(cap, theme::kS3, LV_PART_MAIN);
    return cap;
}

// Krótkie wyjaśnienie pod ustawieniem, zwykłym tekstem, a nie wersalikami.
void add_note(lv_obj_t* body, const char* text) {
    lv_obj_t* note = lv_label_create(body);
    lv_obj_remove_style_all(note);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note, LV_PCT(100));
    lv_obj_set_style_text_font(note, theme::font_r13(), LV_PART_MAIN);
    lv_obj_set_style_text_color(note, theme::dim(), LV_PART_MAIN);
    lv_label_set_text(note, text);
}

}  // namespace

uint32_t screen_timeout_ms() {
    const uint8_t index = storage::get(storage::Pref::ScreenTimeout);
    return index < sizeof(kTimeoutMs) / sizeof(kTimeoutMs[0]) ? kTimeoutMs[index] : kTimeoutMs[1];
}

uint8_t volume_limit() {
    const uint8_t limit = storage::get(storage::Pref::VolumeLimit);
    return limit == 0 || limit > 100 ? 100 : limit;
}

uint8_t effective_brightness() {
    const uint8_t chosen = storage::get(storage::Pref::Brightness);
    const uint8_t night = storage::get(storage::Pref::NightMode);
    if (night == 0 || night >= sizeof(kNightWindows) / sizeof(kNightWindows[0]) ||
        !transport::time_known()) {
        return chosen;
    }
    const std::time_t now = std::time(nullptr);
    std::tm parts = {};
    localtime_r(&now, &parts);
    const NightWindow& window = kNightWindows[night];
    const bool at_night = parts.tm_hour >= window.from || parts.tm_hour < window.to;
    return at_night && chosen > kNightBrightness ? kNightBrightness : chosen;
}

void apply_timezone() {
    const uint8_t index = storage::get(storage::Pref::TimeZone);
    const std::size_t count = sizeof(kTimeZones) / sizeof(kTimeZones[0]);
    transport::set_timezone(kTimeZones[index < count ? index : 0].posix);
}

void build_device(Context& context) {
    DeviceScreens& screen = context.device;

    screen.display = widgets::make_page("Ekran", &on_back_display, &context);
    add_section(screen.display.body, "JASNOŚĆ");
    screen.brightness = widgets::make_segmented(screen.display.body, LV_SYMBOL_EYE_OPEN, 5, &on_brightness,
                                                &context);
    screen.brightness.set_labels(kBrightnessLabels, 5);
    add_section(screen.display.body, "WYGASZANIE PO BEZCZYNNOŚCI");
    screen.timeout = widgets::make_segmented(screen.display.body, LV_SYMBOL_POWER, 5, &on_timeout,
                                             &context);
    screen.timeout.set_labels(kTimeoutLabels, 5);
    add_section(screen.display.body, "PRZYCIEMNIANIE PRZED WYGASZENIEM");
    screen.dim = widgets::make_segmented(screen.display.body, LV_SYMBOL_EYE_CLOSE, 2, &on_dim,
                                         &context);
    screen.dim.set_labels(kDimLabels, 2);
    add_section(screen.display.body, "TRYB NOCNY (JASNOŚĆ NAJWYŻEJ 20%)");
    screen.night = widgets::make_segmented(screen.display.body, LV_SYMBOL_EYE_CLOSE, 4, &on_night,
                                           &context);
    screen.night.set_labels(kNightLabels, 4);

    screen.place = widgets::make_page("Pokój i czas", &on_back_place, &context);
    screen.room = widgets::make_list_row(screen.place.body, LV_SYMBOL_HOME, "Nazwa pokoju",
                                         "w nagłówku ekranów TV, Apple TV, Dźwięk, Dom", "",
                                         &on_room, &context);
    add_section(screen.place.body, "STREFA CZASOWA");
    screen.timezone = widgets::make_segmented(screen.place.body, LV_SYMBOL_GPS, 4, &on_timezone,
                                              &context);
    screen.timezone.set_labels(kTimeZoneLabels, 4);

    screen.input = widgets::make_page("Klawiatura i gładzik", &on_back_input, &context);
    add_section(screen.input.body, "STYL KLAWIATURY");
    screen.keyboard_style = widgets::make_segmented(screen.input.body, LV_SYMBOL_KEYBOARD, 2,
                                                    &on_keyboard_style, &context);
    screen.keyboard_style.set_labels(kStyleLabels, 2);
    add_section(screen.input.body, "TEKST NA APPLE TV");
    screen.keyboard_live = widgets::make_segmented(screen.input.body, LV_SYMBOL_UPLOAD, 2,
                                                   &on_keyboard_live, &context);
    screen.keyboard_live.set_labels(kLiveLabels, 2);
    add_section(screen.input.body, "KLAWIATURA PRZY POLU TEKSTOWYM NA APPLE TV");
    screen.keyboard_auto = widgets::make_segmented(screen.input.body, LV_SYMBOL_DOWN, 2,
                                                   &on_keyboard_auto, &context);
    screen.keyboard_auto.set_labels(kAutoLabels, 2);
    add_note(screen.input.body,
             "Sama: wysuwa się, gdy Apple TV pokaże pole tekstowe. Przyciskiem: tylko po przycisku "
             "Tekst na ekranie Apple TV. Przycisk działa w obu trybach.");
    add_section(screen.input.body, "CZUŁOŚĆ GŁADZIKA");
    screen.trackpad = widgets::make_segmented(screen.input.body, LV_SYMBOL_SHUFFLE, 3,
                                              &on_trackpad, &context);
    screen.trackpad.set_labels(kTrackpadLabels, 3);
    add_note(screen.input.body,
             "Czuły: krok co 20 px ruchu palcem. Zwykły: co 26 px. Spokojny: co 34 px.");

    screen.diagnostics = widgets::make_page("Diagnostyka", &on_back_diagnostics, &context);
    screen.diag_ip = widgets::make_readout(screen.diagnostics.body, "Adres IP");
    screen.diag_link = widgets::make_readout(screen.diagnostics.body, "Łącze z domem");
    screen.diag_z906 = widgets::make_readout(screen.diagnostics.body, "Z906");
    screen.diag_ha = widgets::make_readout(screen.diagnostics.body, "Home Assistant");
    screen.diag_appletv = widgets::make_readout(screen.diagnostics.body, "Apple TV");
    screen.diag_revision = widgets::make_readout(screen.diagnostics.body, "Rewizja stanu");
    add_note(screen.diagnostics.body,
             "Pełny log pilota idzie do MQTT (pilot_debug/<id>/log), "
             "patrz docs/operations/ai-handoff.md.");

    screen.sound = widgets::make_page("Dźwięk", &on_back_sound, &context);
    add_section(screen.sound.body, "NAJWYŻSZA GŁOŚNOŚĆ Z906 Z TARCZY");
    screen.volume_limit = widgets::make_segmented(screen.sound.body, LV_SYMBOL_VOLUME_MAX, 5,
                                                  &on_volume_limit, &context);
    screen.volume_limit.set_labels(kVolumeLimitLabels, 5);
    add_note(screen.sound.body,
             "Tarcza nie wyśle więcej niż limit. Głośności ustawionej inaczej "
             "(pilot Logitech, aplikacja) pilot nie zmienia.");

    screen.about = widgets::make_page("O urządzeniu", &on_back_about, &context);
    screen.version = widgets::make_readout(screen.about.body, "Wersja");
    screen.built = widgets::make_readout(screen.about.body, "Zbudowano");
    screen.idf = widgets::make_readout(screen.about.body, "ESP-IDF");
    screen.memory = widgets::make_readout(screen.about.body, "Wolna pamięć");
    screen.uptime = widgets::make_readout(screen.about.body, "Czas pracy");
    lv_obj_t* restart = widgets::make_action(screen.about.body, LV_SYMBOL_REFRESH,
                                             "Uruchom ponownie", false,
                                             widgets::ActionLayout::Block, &on_restart, &context);
    lv_obj_set_style_margin_top(restart, theme::kS4, LV_PART_MAIN);
    widgets::make_action(screen.about.body, LV_SYMBOL_TRASH, "Przywróć ustawienia fabryczne",
                         false, widgets::ActionLayout::Block, &on_factory_reset, &context);

    const esp_app_desc_t* app = esp_app_get_description();
    screen.version.set(app->version, true);
    char built[40];
    std::snprintf(built, sizeof(built), "%s %s", app->date, app->time);
    screen.built.set(built, true);
    screen.idf.set(app->idf_ver, true);
}

void render_device(Context& context) {
    DeviceScreens& screen = context.device;

    screen.brightness.set_selected(
        index_of(kBrightness, storage::get(storage::Pref::Brightness), 4));
    const uint8_t timeout = storage::get(storage::Pref::ScreenTimeout);
    screen.timeout.set_selected(timeout < 5 ? timeout : 1);
    screen.dim.set_selected(storage::enabled(storage::Pref::DimBeforeSleep) ? 1 : 0);
    screen.volume_limit.set_selected(index_of(kVolumeLimits, volume_limit(), 4));
    const uint8_t night = storage::get(storage::Pref::NightMode);
    screen.night.set_selected(night < 4 ? night : 0);

    lv_label_set_text(screen.room.value, context.room.c_str());
    const uint8_t zone = storage::get(storage::Pref::TimeZone);
    screen.timezone.set_selected(zone < 4 ? zone : 0);

    screen.keyboard_style.set_selected(storage::enabled(storage::Pref::KeyboardQwerty) ? 1 : 0);
    screen.keyboard_live.set_selected(storage::enabled(storage::Pref::KeyboardLive) ? 0 : 1);
    screen.keyboard_auto.set_selected(storage::enabled(storage::Pref::KeyboardAutoOpen) ? 0 : 1);
    screen.trackpad.set_selected(index_of(kTrackpadSteps, storage::get(storage::Pref::TrackpadStep), 1));

    if (!lv_obj_has_flag(screen.diagnostics.root, LV_OBJ_FLAG_HIDDEN)) {
        render_diagnostics(context);
    }

    // Pamięć i czas pracy zmieniają się ciągle, więc liczymy je tylko wtedy,
    // gdy strona jest widoczna.
    if (!lv_obj_has_flag(screen.about.root, LV_OBJ_FLAG_HIDDEN)) {
        char text[48];
        std::snprintf(text, sizeof(text), "%u kB wewn. · %u kB PSRAM",
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        screen.memory.set(text, true);
        const unsigned minutes = static_cast<unsigned>(esp_timer_get_time() / 60000000);
        std::snprintf(text, sizeof(text), "%u h %02u min", minutes / 60, minutes % 60);
        screen.uptime.set(text, true);
    }
}

void open_display_page(Context& context) {
    widgets::show_page(context.device.display, true);
    render(context);
}

void open_sound_page(Context& context) {
    widgets::show_page(context.device.sound, true);
    render(context);
}

void open_place_page(Context& context) {
    widgets::show_page(context.device.place, true);
    render(context);
}

void open_input_page(Context& context) {
    widgets::show_page(context.device.input, true);
    render(context);
}

void open_diagnostics_page(Context& context) {
    widgets::show_page(context.device.diagnostics, true);
    render(context);
}

void open_about_page(Context& context) {
    widgets::show_page(context.device.about, true);
    render(context);
}

}  // namespace ui
