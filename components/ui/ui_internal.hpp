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
    // Poziom: oba segmenty w nagłówku powłoki, widoczne tylko na ekranie TV.
    lv_obj_t* header = nullptr;
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
    // Poziom: oba segmenty w nagłówku powłoki, widoczne tylko na ekranie Z906.
    lv_obj_t* header = nullptr;
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
    // Poziom (wariant C): kafelek steruje też sam — zasilanie, tryb i −/+ bez podstrony.
    // Seria stuknięć w −/+ idzie jednym poleceniem 0,7 s po ostatnim, jak na tarczy.
    lv_obj_t* tile_power = nullptr;
    lv_obj_t* tile_pill = nullptr;
    lv_obj_t* tile_steps = nullptr;
    lv_timer_t* tile_timer = nullptr;
    int32_t tile_pending = 0;

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
    lv_obj_t* mode_icons[domain::kMaxOptions]{};  // poziom: ikona nad nazwą trybu

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
    // W domu (ADR 0015): średnia temperatura i wilgotność z czujników pomieszczeń.
    // Poziom — kafel obok pogody, pion — pigułka pod pogodą.
    lv_obj_t* indoor = nullptr;
    lv_obj_t* indoor_temp = nullptr;
    lv_obj_t* indoor_note = nullptr;
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

// ---------- Ekran Dom: pomieszczenia (ADR 0014) ----------
inline constexpr std::size_t kRoomCount = 6;
inline constexpr std::size_t kKelvinSteps = 7;
inline constexpr std::size_t kEffectTiles = domain::kMaxEffects + 1;  // + „Brak”

struct HomeScreen {
    lv_obj_t* section = nullptr;

    // Klimatyzacja: pion — wąski pasek nad siatką, poziom — pigułka w nagłówku.
    lv_obj_t* climate = nullptr;
    lv_obj_t* climate_dot = nullptr;
    lv_obj_t* climate_text = nullptr;

    // Kafelek pomieszczenia: dotknięcie otwiera pokój, lampka w rogu przełącza
    // jego główne światło.
    struct RoomTile {
        lv_obj_t* root = nullptr;
        lv_obj_t* lamp = nullptr;
        lv_obj_t* lamp_icon = nullptr;
        lv_obj_t* lamp_bar = nullptr;
        lv_obj_t* lamp_fill = nullptr;
        lv_obj_t* dot = nullptr;
        lv_obj_t* state = nullptr;
    };
    RoomTile tiles[kRoomCount];
};

// Podstrona pomieszczenia: pokój → światło (grupa) → żarówka albo lista efektów.
// Pełny ekran. Pion: pasek pomieszczeń pod nagłówkiem. Poziom: pasek zakładek i pasek
// pomieszczeń schowane w szufladach — wysuwa je przeciągnięcie od lewej / prawej krawędzi.
struct RoomScreen {
    enum class Level : uint8_t { Room, Light, Bulb };

    bool built = false;
    bool open = false;
    std::size_t room = 0;
    std::size_t bulb = 0;
    Level stack[3]{};
    std::size_t depth = 0;

    lv_obj_t* root = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* sub = nullptr;
    lv_obj_t* rail_items[kRoomCount]{};
    lv_obj_t* body = nullptr;

    // Poziom: szuflady przy krawędziach, przyciemnienie pod nimi i pasy chwytu gestu.
    lv_obj_t* scrim = nullptr;
    lv_obj_t* drawer_tabs = nullptr;
    lv_obj_t* drawer_rooms = nullptr;
    lv_obj_t* edges[2]{};
    int8_t drawer = 0;      // -1 lewa (zakładki), 1 prawa (pomieszczenia), 0 schowane
    int8_t swipe = 0;       // krawędź, od której zaczął się gest; 0 = brak
    int32_t swipe_x = 0;

    // Poziom 1: urządzenia pokoju.
    struct Card {
        lv_obj_t* root = nullptr;
        lv_obj_t* well = nullptr;
        lv_obj_t* icon = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* state = nullptr;
        lv_obj_t* note = nullptr;
        lv_obj_t* power = nullptr;
        lv_obj_t* chevron = nullptr;
    };
    lv_obj_t* view_room = nullptr;
    // Karta „W pokoju” (ADR 0017): temperatura i wilgotność z czujnika pokoju.
    struct Reading {
        lv_obj_t* root = nullptr;
        lv_obj_t* value = nullptr;
        lv_obj_t* unit = nullptr;
        lv_obj_t* dot = nullptr;
        lv_obj_t* verdict = nullptr;
    };
    lv_obj_t* clima_cap = nullptr;     // pion: `W POKOJU` nad kartą
    lv_obj_t* devices_cap = nullptr;   // `URZĄDZENIA` (poziom: `W POKOJU · URZĄDZENIA`)
    lv_obj_t* clima = nullptr;
    lv_obj_t* clima_sep = nullptr;
    Reading temperature;
    Reading humidity;
    uint32_t shown_weather = 0;        // rewizja pogody pokazana na karcie
    std::size_t shown_clima_room = kRoomCount;
    Card lamp;
    Card climate;
    Card empty;

    // Poziom 2 i 3: światło — grupa albo jedna żarówka. Układ jak podstrona klimy:
    // tarcza jasności i obok kafle barwy, efektu i żarówek.
    lv_obj_t* view_light = nullptr;
    widgets::Thermostat* dial = nullptr;
    lv_obj_t* kelvin_row = nullptr;
    lv_obj_t* light_power = nullptr;
    lv_obj_t* kelvin_box = nullptr;
    lv_obj_t* kelvin_value = nullptr;
    lv_obj_t* kelvin[kKelvinSteps]{};
    uint16_t kelvin_steps[kKelvinSteps]{};
    lv_obj_t* group_box = nullptr;      // efekt i żarówki: tylko dla grupy
    // Efekty jako kafle (jak tryby klimy): bieżący na dużym kaflu, obok najczęściej wybierane
    // na tej lampie (przy włączonym efekcie pierwszy to „Brak”), ostatni „Więcej” otwiera listę.
    // Poziom: duży przez dwa rzędy, 4 + Więcej na całą wysokość; pion (mniej miejsca pod
    // tarczą): jeden rząd — bieżący, 2 + Więcej.
    lv_obj_t* effect_cap = nullptr;
    lv_obj_t* effect_grid = nullptr;
    bool effect_wide = false;
    struct EffectCell {
        lv_obj_t* root = nullptr;
        lv_obj_t* icon = nullptr;
        lv_obj_t* label = nullptr;
        Context* context = nullptr;
        domain::EffectName name;
        bool more = false;
    };
    static constexpr std::size_t kEffectCells = 6;  // duży, cztery małe, „Więcej”
    EffectCell effect_cells[kEffectCells];
    struct BulbRow {
        lv_obj_t* root = nullptr;
        lv_obj_t* icon = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* note = nullptr;
        Context* context = nullptr;
        std::size_t index = 0;
    };
    BulbRow bulbs[domain::kMaxLightMembers];

    // Wybór efektu dla całej grupy: lista jak wybór trybu klimy — w poziomie panel przy
    // prawej krawędzi, w pionie pełny ekran. Wybór zamyka listę. Pozycja 0 to „Brak”.
    widgets::Page effect_picker;
    bool effect_picker_open = false;
    struct EffectTile {
        widgets::ListRow row;
        lv_obj_t* icon = nullptr;
        Context* context = nullptr;
        std::size_t index = 0;
        domain::EffectName name;
    };
    EffectTile effects[kEffectTiles];
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
    widgets::Segmented orientation;  // Pref::Orientation; zmiana restartuje pilota
    widgets::Segmented theme;
    lv_obj_t* theme_hours_cap = nullptr;  // widoczne tylko w trybie „wg godzin”
    widgets::Segmented theme_hours;
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
    lv_obj_t* identity = nullptr;  // wiersz nagłówka; TV w poziomie dokłada tu źródło i obraz
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
    RoomScreen room_page;
    ClimateScreen climate;
    StartScreen start;
    MoreScreen more;
    DeviceScreens device;
};

namespace home {
// Karta „W pokoju” na podstronie pomieszczenia (ADR 0017), screens/home_room.cpp.
void render_room_climate(Context& context, bool force);
}  // namespace home

// Wysyła zamiar i od razu odświeża widok — dotknięcie nie czeka na sieć.
void dispatch(Context& context, const domain::UiIntent& intent);

// Zamiar z rodzajem i kontrolką; resztę pól (wartość, cel, tekst) ustawia wołający.
inline domain::UiIntent make_intent(domain::IntentKind kind,
                                    domain::ControlId control = domain::ControlId::None) {
    domain::UiIntent intent;
    intent.kind = kind;
    intent.control = control;
    return intent;
}

// Przerysowuje wszystko ze stanu. Jedyna droga, którą dane trafiają na ekran.
void render(Context& context);
void show_screen(Context& context, ScreenId screen);

// Poziom: miejsce na kontrolki ekranu w wierszu nagłówka (po nazwie, przed Decode
// i zasilaniem), np. źródło i tryb obrazu TV. Ukryte, dopóki ekran go nie pokaże.
lv_obj_t* make_header_slot(Context& context, int32_t width);
// Widoczne tylko na ekranie `owner`; bez świeżego stanu przygaszone jak sekcja ekranu
// (dotyk blokują wtedy same handlery).
void show_header_slot(const Context& context, lv_obj_t* slot, ScreenId owner, bool fresh);

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
// Podstrona pomieszczenia (screen_home.cpp). Zamykana przy zmianie ekranu i wybudzeniu.
void close_room(Context& context);
// Podstrona klimatyzacji, także z karty w pomieszczeniu (screen_climate.cpp).
void open_climate_page(Context& context);
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
// Poziom: podstrony ustawień zadokowane obok listy (screen_more.cpp). W pionie nic nie robią
// poza zamykaniem.
void dock_settings_pages(Context& context);
void close_settings_pages(Context& context);
// Po zmianie ekranu: poza ustawieniami zamyka podstronę, w ustawieniach dba, żeby jedna była otwarta.
void sync_settings_detail(Context& context);
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
// Styl, jaki interfejs ma mieć teraz: z ustawień, w trybie „wg godzin” zależny od pory.
theme::Mode effective_theme();
// Strefa czasowa z ustawień — wołać przed pierwszą synchronizacją czasu.
void apply_timezone();
// Czas wygaszania z ustawień w milisekundach; 0 = nigdy.
uint32_t screen_timeout_ms();
// Najwyższa głośność Z906, jaką tarcza może wysłać.
uint8_t volume_limit();


}  // namespace ui
