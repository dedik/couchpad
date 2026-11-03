#pragma once

// Wspólny stan interfejsu: powłoka, pięć ekranów i wiązanie z kontrolerem.
// Plik jest prywatny dla komponentu `ui`.

#include "application/app_controller.hpp"
#include "application/network_controller.hpp"
#include "storage/settings.hpp"
#include "ui/ui.hpp"
#include "lvgl.h"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace ui {

enum class ScreenId : uint8_t { Tv, AppleTv, Audio, Home, More, kCount };
inline constexpr std::size_t kScreenCount = static_cast<std::size_t>(ScreenId::kCount);

struct Context;

// ---------- Ekran TV ----------
struct TvScreen {
    lv_obj_t* section = nullptr;
    widgets::Banner banner;
    // Źródło i tryb obrazu w jednym rzędzie: każdy to jeden segment z bieżącą
    // wartością, a dotknięcie otwiera listę wyboru. Trybów bywa osiem, a nazwy
    // w stylu „Dolby Vision Cinema Bright” nie mieszczą się w rzędzie segmentów.
    widgets::Segmented source;
    widgets::Segmented picture;
    widgets::Tiles channels;

    // Wspólna lista wyboru dla źródła i trybu obrazu.
    struct PickerRow {
        widgets::ListRow row;
        Context* context = nullptr;
        std::size_t index = 0;
    };
    widgets::Page picker;
    PickerRow picker_rows[domain::kMaxOptions];
    domain::ControlId picker_control = domain::ControlId::None;

    // Menu wszystkich kanałów tunera (ADR 0008). Wiersze powstają przy pierwszym
    // otwarciu, w liczbie kanałów z listy, i są przepisywane tylko po jej zmianie.
    // Wiersz kanału to przycisk i dwie etykiety, nie pełny ListRow (7 obiektów):
    // kanałów bywa blisko setki, a pierwsze otwarcie buduje je wszystkie naraz.
    struct ChannelRow {
        lv_obj_t* root = nullptr;
        lv_obj_t* number = nullptr;
        lv_obj_t* name = nullptr;
        Context* context = nullptr;
        std::size_t index = 0;
    };
    widgets::Page channel_page;
    lv_obj_t* channel_empty = nullptr;
    ChannelRow channel_rows[domain::kMaxChannels];
    std::size_t channel_rows_built = 0;
    uint32_t channel_revision_shown = 0;
    bool channel_page_open = false;
};

// ---------- Ekran Apple TV ----------
struct AppleTvScreen {
    lv_obj_t* section = nullptr;
    widgets::Banner banner;
    widgets::Tiles apps;

    // Klawiatura wysuwa się, gdy Apple TV zgłosi pole tekstowe. Zapamiętujemy
    // poprzedni stan, żeby otworzyć ją raz, na zboczu, a nie przy każdym rysowaniu.
    bool keyboard_seen = false;
    bool keyboard_open = false;
    // Otwarta przyciskiem, nie przez Apple TV: nie zamyka się, gdy pole tekstowe
    // na TV zniknie albo jeszcze się nie pojawiło.
    bool keyboard_manual = false;
    int64_t last_step_ms = 0;
};

// ---------- Ekran dźwięku ----------
struct AudioScreen {
    lv_obj_t* section = nullptr;
    widgets::Banner banner;
    // Wejście i efekt w jednym rzędzie, jak źródło i tryb obrazu na ekranie TV:
    // segment pokazuje aktywną opcję, dotknięcie otwiera listę wyboru.
    widgets::Segmented source;
    widgets::Segmented effect;
    widgets::Dial* dial = nullptr;
    lv_obj_t* mute = nullptr;
    widgets::Level* center = nullptr;
    widgets::Level* subwoofer = nullptr;
    widgets::Level* rear = nullptr;

    struct PickerRow {
        widgets::ListRow row;
        Context* context = nullptr;
        std::size_t index = 0;
    };
    widgets::Page picker;
    PickerRow picker_rows[domain::kMaxOptions];
    domain::ControlId picker_control = domain::ControlId::None;
};

// ---------- Klimatyzacja: kafelek na ekranie Dom i podstrona ----------
struct ClimateScreen {
    // Kafelek pokazuje stan, nie steruje: dotknięcie otwiera podstronę.
    lv_obj_t* tile = nullptr;
    lv_obj_t* tile_dot = nullptr;
    lv_obj_t* tile_temp_box = nullptr;
    lv_obj_t* tile_temp = nullptr;
    lv_obj_t* tile_unit = nullptr;
    // W trybie wentylatora temperatura nie działa — jej miejsce zajmuje moc nawiewu.
    lv_obj_t* tile_fan_box = nullptr;
    lv_obj_t* tile_fan = nullptr;
    lv_obj_t* tile_mode = nullptr;
    lv_obj_t* tile_now = nullptr;

    // Podstrona powstaje przy pierwszym otwarciu — wolnej pamięci po starcie
    // zostaje niewiele, a do klimy można nie zajrzeć wcale.
    bool built = false;
    widgets::Page page;
    lv_obj_t* power = nullptr;
    widgets::Banner banner;
    widgets::Thermostat* thermo = nullptr;
    // Tryby bez `off` — wyłączenie to robota przycisku zasilania. Identyfikatory
    // segmentów w kolejności z encji.
    widgets::Segmented modes;
    domain::Label mode_ids[domain::kMaxOptions];
    std::size_t mode_count = 0;

    // Moc nawiewu i żaluzje: kafelek z bieżącą wartością, dotknięcie otwiera listę.
    struct Option {
        lv_obj_t* root = nullptr;
        lv_obj_t* value = nullptr;
    };
    Option fan;
    Option swing;
    Option swing_h;

    struct PickerRow {
        widgets::ListRow row;
        Context* context = nullptr;
        std::size_t index = 0;
    };
    widgets::Page picker;
    PickerRow picker_rows[domain::kMaxOptions];
    domain::ControlId picker_control = domain::ControlId::None;
};

// ---------- Ekran startowy (ADR 0010) ----------
// Po starcie i po każdym wybudzeniu, nigdy z nawigacji. Nie steruje niczym, więc
// wychodzi od razu, a synchronizacja biegnie w tle.
struct StartScreen {
    lv_obj_t* root = nullptr;      // na lv_layer_sys, nad podstronami
    lv_obj_t* time = nullptr;
    lv_obj_t* date = nullptr;
    // Pogoda obok zegara (ADR 0011): ikona, temperatura i nazwa warunku.
    lv_obj_t* weather = nullptr;
    lv_obj_t* weather_icon = nullptr;
    lv_obj_t* weather_temp = nullptr;
    lv_obj_t* weather_text = nullptr;
    uint32_t shown_weather = 0;    // rewizja pogody na ekranie; 0 = nic
    struct Tile {
        lv_obj_t* root = nullptr;
        lv_obj_t* dot = nullptr;
        lv_obj_t* state = nullptr;
    };
    Tile tiles[4];
    lv_obj_t* link_dot = nullptr;
    lv_obj_t* link_text = nullptr;
    lv_obj_t* retry = nullptr;
    bool open = false;
    int shown_minute = -1;         // zegar przerysowujemy tylko po zmianie minuty
};

// ---------- Ekran Dom ----------
struct HomeScreen {
    lv_obj_t* section = nullptr;
    widgets::Tiles scenes;
};

// ---------- Ekran Więcej ----------
struct MoreScreen {
    lv_obj_t* section = nullptr;
    widgets::ListRow wifi;
    widgets::ListRow mqtt;
    widgets::ListRow brightness;  // podstrona „Ekran”
    widgets::ListRow sleep;       // podstrona „Dźwięk”
    widgets::ListRow place;       // podstrona „Pokój i czas”
    widgets::ListRow input;       // podstrona „Klawiatura i gładzik”
    widgets::ListRow diagnostics;
    widgets::ListRow about;
};

// ---------- Podstrony ustawień pilota ----------
struct DeviceScreens {
    widgets::Page display;
    widgets::Segmented brightness;
    widgets::Segmented timeout;
    widgets::Segmented dim;
    widgets::Segmented night;

    widgets::Page place;            // „Pokój i czas”
    widgets::ListRow room;
    widgets::Segmented timezone;

    widgets::Page input;            // „Klawiatura i gładzik”
    widgets::Segmented keyboard_style;
    widgets::Segmented keyboard_live;
    widgets::Segmented keyboard_auto;
    widgets::Segmented trackpad;

    widgets::Page diagnostics;
    widgets::Readout diag_ip;
    widgets::Readout diag_link;
    widgets::Readout diag_z906;
    widgets::Readout diag_ha;
    widgets::Readout diag_appletv;
    widgets::Readout diag_revision;

    widgets::Page sound;
    widgets::Segmented volume_limit;

    widgets::Page about;
    widgets::Readout version;
    widgets::Readout built;
    widgets::Readout idf;
    widgets::Readout memory;
    widgets::Readout uptime;
};

// ---------- Podstrona Wi-Fi ----------
inline constexpr std::size_t kMaxNetworkRows = transport::kMaxAccessPoints;

struct WifiScreen {
    widgets::Page page;
    widgets::Readout status;
    widgets::Readout signal;       // siła sygnału przy połączeniu
    widgets::Readout link;         // zerwania i nieudane próby od startu
    widgets::ListRow current;      // sieć połączona albo zapamiętana
    lv_obj_t* rescan = nullptr;
    lv_obj_t* hint = nullptr;
    lv_obj_t* list = nullptr;
    widgets::ListRow rows[kMaxNetworkRows];

    // Sieć, dla której edytor zbiera hasło.
    transport::Ssid selected_ssid;
    bool open = false;
};

// ---------- Podstrona MQTT ----------
struct MqttScreen {
    widgets::Page page;
    widgets::Readout status;
    widgets::ListRow host;
    widgets::ListRow port;
    widgets::ListRow username;
    widgets::ListRow password;
    widgets::ListRow z906_id;
    widgets::ListRow remote_id;  // kontrakt v1 z Home Assistantem (Apple TV)
    widgets::ListRow tv_prefix;  // Glasshouse na telewizorze (ADR 0008)
    lv_obj_t* apply = nullptr;

    storage::MqttSettings draft;
};

// ---------- Powłoka ----------
struct Shell {
    lv_obj_t* root = nullptr;
    lv_obj_t* topbar = nullptr;
    // Pasek statusu: godzina z sieci i stan łącza. Mieści się wewnątrz nagłówka,
    // żeby nie zabierać wysokości ekranom.
    lv_obj_t* ident_icon = nullptr;
    lv_obj_t* ident_name = nullptr;
    lv_obj_t* ident_room = nullptr;
    lv_obj_t* power = nullptr;
    // Wskaźnik Decode 5.1 obok zasilania, tylko na ekranie dźwięku. Informacja,
    // nigdy przełącznik.
    lv_obj_t* decode = nullptr;
    lv_obj_t* decode_dot = nullptr;
    lv_obj_t* decode_label = nullptr;
    lv_obj_t* content = nullptr;
    lv_obj_t* pages[kScreenCount]{};
    lv_obj_t* tabbar = nullptr;
    lv_obj_t* tabs[kScreenCount]{};

    // Nakładka wybudzania nad wszystkim, zdejmowana dopiero po nowym snapshocie.
    lv_obj_t* wake = nullptr;
    lv_obj_t* wake_mark = nullptr;
    lv_obj_t* wake_title = nullptr;
    lv_obj_t* wake_note = nullptr;
    lv_obj_t* wake_steps[3]{};
    lv_obj_t* wake_dots[3]{};
    lv_obj_t* wake_retry = nullptr;
    lv_obj_t* wake_settings = nullptr;
};

struct Context {
    application::AppController* controller = nullptr;
    ScreenId screen = ScreenId::Tv;

    application::NetworkController* network = nullptr;
    application::LiveSource* live = nullptr;
    DisplayPower display_power = nullptr;
    DisplayLevel display_brightness = nullptr;
    DisplayLevel display_dim = nullptr;

    // Jedna nakładka z klawiaturą na wszystkie pola w programie.
    widgets::TextEditor* editor = nullptr;

    // Wygaszanie po bezczynności. Pierwszy dotyk po zgaszeniu ma tylko wybudzić,
    // więc łapie go osobna warstwa, zamiast trafić w kontrolkę pod palcem.
    lv_obj_t* sleep_blocker = nullptr;
    bool asleep = false;
    // Przyciemnienie przed wygaszeniem (ustawienie „Ekran”). Dotyk w tym czasie
    // działa normalnie — ekran jest tylko ciemniejszy.
    bool dimmed = false;
    // Jasność ostatnio podana płytce; -1 wymusza ponowne ustawienie.
    int applied_brightness = -1;
    // Nazwa pokoju w nagłówku ekranów urządzeń (ustawienie „Pokój i czas”).
    storage::RoomName room;

    Shell shell;
    TvScreen tv;
    WifiScreen wifi;
    MqttScreen mqtt;
    AppleTvScreen appletv;
    AudioScreen audio;
    HomeScreen home;
    ClimateScreen climate;
    StartScreen start;
    MoreScreen more;
    DeviceScreens device;
};

// Wysyła zamiar i od razu odświeża widok — dotknięcie nie czeka na sieć.
void dispatch(Context& context, const domain::UiIntent& intent);

// Przerysowuje wszystko ze stanu. Jedyna droga, którą dane trafiają na ekran.
void render(Context& context);
void show_screen(Context& context, ScreenId screen);

// Powłoka.
void build_shell(Context& context);
void render_shell(Context& context);

// Ekrany.
void build_tv(Context& context, lv_obj_t* parent);
void render_tv(Context& context);
void build_appletv(Context& context, lv_obj_t* parent);
void render_appletv(Context& context);
void build_audio(Context& context, lv_obj_t* parent);
void render_audio(Context& context);
void build_home(Context& context, lv_obj_t* parent);
void render_home(Context& context);
// Kafelek klimy na ekranie Dom i jej podstrona (screen_climate.cpp).
void build_climate_tile(Context& context, lv_obj_t* parent);
void render_climate(Context& context);
// Polska nazwa trybu klimy z identyfikatora HA (`cool` → „Chłodzenie”).
const char* climate_mode_name(const char* id);
// Ekran startowy (screen_start.cpp). `show_start(true)` zamyka podstrony i okna,
// żeby po wyborze kafelka nie przykryły ekranu urządzenia.
void build_start(Context& context);
void render_start(Context& context);
void show_start(Context& context, bool visible);
void build_more(Context& context, lv_obj_t* parent);
void render_more(Context& context);
void build_wifi(Context& context);
void render_wifi(Context& context);
void open_wifi_page(Context& context);
void build_mqtt(Context& context);
void render_mqtt(Context& context);
void open_mqtt_page(Context& context);
// Podstrony ustawień pilota: Ekran, Dźwięk, O urządzeniu.
void build_device(Context& context);
void render_device(Context& context);
void open_display_page(Context& context);
void open_sound_page(Context& context);
void open_about_page(Context& context);
void open_place_page(Context& context);
void open_input_page(Context& context);
void open_diagnostics_page(Context& context);
// Jasność, jaką ekran ma mieć teraz: z ustawień, w nocy obniżona (tryb nocny).
uint8_t effective_brightness();
// Strefa czasowa z ustawień — wołać przed pierwszą synchronizacją czasu.
void apply_timezone();
// Czas wygaszania z ustawień w milisekundach; 0 = nigdy.
uint32_t screen_timeout_ms();
// Najwyższa głośność Z906, jaką tarcza może wysłać.
uint8_t volume_limit();


}  // namespace ui
