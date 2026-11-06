#pragma once

// Komponenty systemu Pulpit w wersji LVGL. Zgodnie z tabelą „Mapowanie na LVGL"
// z docs/ux/mockup-pulpit/system.html. Nowy ekran nie wprowadza nowych kształtów —
// dobiera je stąd.

#include <cstddef>
#include <cstdint>

#include "domain/app_state.hpp"
#include "lvgl.h"
#include "ui/theme.hpp"

namespace ui::widgets {

// Wspólny kształt wywołania zwrotnego: kontekst plus to, co zdarzenie niesie.
using IndexHandler = void (*)(void* context, std::size_t index);
using ValueHandler = void (*)(void* context, int32_t value, bool final_value);
using PlainHandler = void (*)(void* context);
using KeyHandler = void (*)(void* context, domain::NavKey key);
using PressHandler = void (*)(void* context, domain::Press press);

// ---------- Sekcja ekranu ----------
// Treść ekranu wypełnia wyłącznie strefę środkową powłoki.
lv_obj_t* make_section(lv_obj_t* parent);

// Rozpychacz: zajmuje całą wolną wysokość i centruje swoje dziecko.
lv_obj_t* make_fill(lv_obj_t* parent);

// Etykieta sekcji wersalikami.
lv_obj_t* make_cap(lv_obj_t* parent, const char* text);

// ---------- Grupa wyboru (.group + .seg) ----------
struct Segmented {
    lv_obj_t* root = nullptr;
    lv_obj_t* box = nullptr;
    lv_obj_t* items[domain::kMaxOptions]{};
    std::size_t count = 0;

    // Ustawia podpisy z listy opcji ze snapshotu. Firmware nie zaszywa ich na stałe.
    void set_options(const domain::OptionList& options);
    void set_labels(const char* const* labels, std::size_t count);
    // Zaznaczenie zawsze pochodzi ze stanu, nigdy z dotknięcia.
    void set_selected(std::size_t index);
    void set_selected_none();
    // Obwódka oczekiwania lub braku potwierdzenia na konkretnej pozycji.
    void set_status(std::size_t index, domain::ControlStatus status);
    const char* label_at(std::size_t index) const;
};

Segmented make_segmented(lv_obj_t* parent, const char* leading_symbol, std::size_t slots,
                         IndexHandler on_select, void* context);

// Jeden segment z bieżącą wartością w połowie rzędu (`make_duo`). Dotknięcie otwiera
// listę wyboru — dla opcji, których jest za dużo albo mają za długie nazwy na rząd
// segmentów (źródło i tryb obrazu TV, wejście i efekt Z906). Długą nazwę kończy wielokropek.
// ---------- Układ poziomy (ADR 0013) ----------
// Sekcja ekranu jako wiersz kolumn zamiast kolumny wierszy.
void make_split(lv_obj_t* section, int32_t gap);
// Kolumna na pełną wysokość: stałej szerokości (`width` > 0) albo dzieląca resztę wiersza.
lv_obj_t* make_pane(lv_obj_t* parent, int32_t width, int32_t gap);
// Pusta przestrzeń w kolumnie: dopycha następne elementy do dołu.
lv_obj_t* make_spacer(lv_obj_t* column);

// Niższy segment z wartością, do 64-pikselowego wiersza nagłówka (poziom: TV, Z906).
void make_header_segment(Segmented& segmented);

Segmented make_value_segment(lv_obj_t* row, const char* leading_symbol,
                             IndexHandler on_press, void* context);

// ---------- Kafelki (.tiles + .tile) ----------
struct Tiles {
    lv_obj_t* root = nullptr;
    lv_obj_t* items[6]{};
    std::size_t count = 0;

    void set_selected(std::size_t index);
    void set_selected_none();
    void set_status(std::size_t index, domain::ControlStatus status);
};

struct TileSpec {
    const lv_image_dsc_t* image = nullptr;  // logotyp; ma pierwszeństwo przed symbolem
    const char* symbol = nullptr;
    const char* text = nullptr;
};

Tiles make_tiles(lv_obj_t* parent, const TileSpec* specs, std::size_t count, int32_t columns,
                 int32_t height, IndexHandler on_press, void* context);

// ---------- Tarcza głośności (.dial) ----------
// Domyślny lv_arc skacze do miejsca dotknięcia, a projekt wymaga zmiany względnej,
// więc zdarzenia obsługuje przezroczysta powierzchnia nad łukiem.
struct Dial {
    lv_obj_t* root = nullptr;
    lv_obj_t* arc = nullptr;
    lv_obj_t* readout = nullptr;  // kolumna w środku łuku: wartość i podpis
    lv_obj_t* value_label = nullptr;
    lv_obj_t* caption = nullptr;

    void set_value(int32_t value);
    int32_t value() const { return value_; }
    void set_status(domain::ControlStatus status);

    int32_t value_ = 0;
    float accumulator_ = 0.0F;
    float last_angle_ = 0.0F;
    bool moved_ = false;
    ValueHandler on_change = nullptr;
    void* context = nullptr;
};

Dial* make_dial(lv_obj_t* parent, const char* caption, ValueHandler on_change, void* context);

// ---------- Tarcza temperatury (.thermo) ----------
// Łuk 270° na zakres z encji, otwarty u dołu na przyciski −/+. Obrót palcem zmienia
// wartość względnie, jak tarcza głośności. Wartości w dziesiątych stopnia.
// `on_change` dostaje wartość końcową: po puszczeniu palca albo po chwili bez
// dotyku po serii stuknięć w −/+ — kilka stuknięć to jedno polecenie.
struct Thermostat {
    lv_obj_t* root = nullptr;
    lv_obj_t* ring = nullptr;     // powierzchnia gestu nad łukiem
    lv_obj_t* arc = nullptr;
    lv_obj_t* action_label = nullptr;
    lv_obj_t* value_label = nullptr;
    lv_obj_t* unit_label = nullptr;
    lv_obj_t* now_label = nullptr;
    lv_obj_t* scale_min = nullptr;
    lv_obj_t* scale_max = nullptr;
    lv_obj_t* minus = nullptr;
    lv_obj_t* plus = nullptr;

    void set_range(int32_t min_tenths, int32_t max_tenths, int32_t step_tenths);
    void set_value(int32_t tenths);
    int32_t value() const { return value_; }
    void set_status(domain::ControlStatus status);
    // Wyłączona klima albo sam nawiew: temperatura nie działa, tarcza gaśnie.
    void set_enabled(bool enabled);
    void set_texts(const char* action, const char* now);
    // `true` w trakcie gestu i przed wysłaniem serii stuknięć. Wtedy rysowanie ze
    // stanu nie nadpisuje wartości pod palcem.
    bool busy() const { return moved_ || commit_timer_ != nullptr; }

    int32_t min_ = 160;
    int32_t max_ = 300;
    int32_t step_ = 5;
    int32_t value_ = 0;
    float accumulator_ = 0.0F;
    float last_angle_ = 0.0F;
    bool moved_ = false;
    bool enabled_ = true;
    lv_timer_t* commit_timer_ = nullptr;
    ValueHandler on_change = nullptr;
    void* context = nullptr;
};

Thermostat* make_thermostat(lv_obj_t* parent, ValueHandler on_change, void* context);

// Temperatura z dziesiątych na tekst z przecinkiem: 225 → „22,5”.
void format_tenths(char* out, std::size_t size, int32_t tenths);

// ---------- Kolumna poziomu (.level) ----------
// Nie lv_slider — ten również skacze do dotyku. Gest pionowy obsłużony ręcznie,
// polem chwytu jest cała kolumna.
struct Level {
    lv_obj_t* root = nullptr;
    lv_obj_t* bar = nullptr;
    lv_obj_t* name_label = nullptr;
    lv_obj_t* value_label = nullptr;

    void set_value(int32_t value);
    int32_t value() const { return value_; }
    void set_status(domain::ControlStatus status);

    int32_t value_ = 0;
    float accumulator_ = 0.0F;
    bool moved_ = false;
    ValueHandler on_change = nullptr;
    void* context = nullptr;
};

Level* make_level(lv_obj_t* parent, const char* name, ValueHandler on_change, void* context);

// ---------- Krzyżak (.pad) ----------
lv_obj_t* make_dpad(lv_obj_t* parent, KeyHandler on_key, void* context);

// ---------- Kołyska (.rocker) ----------
// Pionowa para przycisków obok krzyżaka, jak VOL i P na pilocie LG: `+` na górze,
// podpis w środku, `−` na dole. `repeat` powtarza naciśnięcie, gdy palec trzyma przycisk.
lv_obj_t* make_rocker(lv_obj_t* parent, const char* caption, bool repeat, PlainHandler on_up,
                      PlainHandler on_down, void* context);

// ---------- Okrągły przycisk kroku (−/+) ----------
// Ten sam przycisk, co −/+ przy tarczy temperatury: 72 px, tło surface-2. Z `repeat`
// pierwszy krok idzie od razu po dotknięciu, a przytrzymanie powtarza go.
lv_obj_t* make_step_button(lv_obj_t* parent, const char* symbol, bool repeat,
                           PlainHandler on_press, void* context);

// ---------- Gładzik Apple TV (.trackpad) ----------
// `on_hold` dostaje przytrzymanie środka (OK) — na pilocie Siri otwiera ono menu
// kontekstowe. Przytrzymany środek nie wysyła już zwykłego OK po puszczeniu.
// `size` to średnica koła; 0 oznacza domyślne 240 px.
lv_obj_t* make_trackpad(lv_obj_t* parent, KeyHandler on_key, void* context,
                        KeyHandler on_hold = nullptr, int32_t size = 0);

// ---------- Przycisk akcji (.act) ----------
// Układ trzeba podać jawnie. `Fill` rozpycha się w wierszu, więc wolno go użyć
// tylko tam, gdzie rodzic zna swoją szerokość — `flex_grow` w kontenerze liczonym
// z zawartości zapętla układ LVGL.
enum class ActionLayout : uint8_t {
    Fill,     // dzieli szerokość wiersza po równo (.duo)
    Block,    // pełna szerokość rodzica, stała wysokość
    Stacked,  // ikona nad tekstem, stały 84 x 124 (gładzik Apple TV)
};

lv_obj_t* make_action(lv_obj_t* parent, const char* symbol, const char* text, bool primary,
                      ActionLayout layout, PlainHandler on_press, void* context);

// Stuknięcie, podwójne stuknięcie i przytrzymanie na jednym przycisku, jak na
// pilocie Siri. Przycisk tworzy się bez `on_press`, a zdarzenia podpina ta funkcja.
// Pojedyncze stuknięcie czeka na drugie, więc wychodzi z opóźnieniem okna
// podwójnego stuknięcia.
void attach_presses(lv_obj_t* button, PressHandler handler, void* context);

// Dwie akcje obok siebie (.duo).
lv_obj_t* make_duo(lv_obj_t* parent);

// ---------- Wiersz listy (.list-row) ----------
struct ListRow {
    lv_obj_t* root = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* note = nullptr;
    lv_obj_t* value = nullptr;
};

ListRow make_list_row(lv_obj_t* parent, const char* symbol, const char* title, const char* note,
                      const char* value, PlainHandler on_press, void* context);

// ---------- Wiersz informacyjny (.readout) ----------
struct Readout {
    lv_obj_t* root = nullptr;
    lv_obj_t* dot = nullptr;
    lv_obj_t* value = nullptr;

    void set(const char* text, bool on);
};

Readout make_readout(lv_obj_t* parent, const char* name);

// ---------- Pasek braku świeżego stanu (.banner) ----------
// Leży poza układem, więc jego pojawienie się nie przesuwa żadnej kontrolki.
// `blocker` to przezroczysta warstwa, która łyka dotknięcia przygaszonych sterowań.
struct Banner {
    lv_obj_t* blocker = nullptr;
    lv_obj_t* box = nullptr;
};

Banner make_banner(lv_obj_t* parent, const char* text);

// Gasi sterowania ekranu i pokazuje pasek. Zamiast zgadywać wartości.
void set_unavailable(lv_obj_t* section, const Banner& banner, bool unavailable);

// ---------- Przycisk zasilania ----------
lv_obj_t* make_power_button(lv_obj_t* parent, PlainHandler on_press, void* context);
void set_power_state(lv_obj_t* button, bool on, bool enabled);

// ---------- Podstrona drugiego poziomu ----------
// Pasek zakładek obsługuje pięć ekranów i więcej slotów nie dostanie. Wszystko, co
// schodzi głębiej, otwiera się jako pełnoekranowa nakładka z powrotem w nagłówku.
struct Page {
    lv_obj_t* root = nullptr;
    lv_obj_t* body = nullptr;
    lv_obj_t* title = nullptr;
};

Page make_page(const char* title, PlainHandler on_back, void* context);
// Poziom: lista wyboru jako panel 380 px przy prawej krawędzi nad przyciemnionym
// ekranem (makieta docs/ux/mockup-landscape). Dotknięcie obok panelu działa jak Cofnij.
// W pionie nic nie zmienia.
void dock_sheet(const Page& page);
void show_page(const Page& page, bool visible);

// ---------- Pytanie o potwierdzenie ----------
// Okno nad wszystkim (warstwa systemowa, nad podstronami i edytorem), jedno na raz.
// `on_confirm` woła się tylko po wyborze `confirm_label`; „Anuluj” i dotknięcie
// tła zamykają okno bez skutku. Do działań, których nie da się cofnąć jednym ruchem.
void ask_confirm(const char* title, const char* message, const char* confirm_label,
                 PlainHandler on_confirm, void* context);
// Zamyka pytanie bez skutku, jakby wybrano „Anuluj”. Nic nie robi, gdy nie jest otwarte.
void dismiss_confirm();

// ---------- Edytor pola tekstowego ----------
// Jedna nakładka z klawiaturą na cały interfejs: hasło sieci, adres brokera,
// identyfikator pilota. Zasłania ekran, bo w trakcie pisania i tak nie ma czego
// obok dotykać.
enum class EditorMode : uint8_t {
    Text,      // zwykły tekst
    Secret,    // hasło: pokazujemy kropki
    Number,    // klawiatura numeryczna
};

using EditorHandler = void (*)(void* context, const char* value);

struct TextEditor {
    lv_obj_t* layer = nullptr;
    lv_obj_t* caption = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* field = nullptr;
    lv_obj_t* reveal = nullptr;   // podgląd wpisanego hasła
    bool secret = false;

    // Własna klawiatura: zakładki, siatka znaków i dolny rząd. Dwa style —
    // siatka jak na Apple TV albo QWERTY jak na telefonie.
    lv_obj_t* keys = nullptr;
    lv_obj_t* key_tabs = nullptr;
    lv_obj_t* key_grid = nullptr;
    lv_obj_t* style_toggle = nullptr;
    uint8_t key_tab = 0;     // 0 abc, 1 ąę, 2 123, 3 #+=
    bool shift = false;      // wielka litera dla następnego znaku
    bool qwerty = false;

    // Tekst na żywo: pole, które poda `on_change`, dostaje tekst w trakcie
    // pisania, nie dopiero po „Zapisz”.
    lv_obj_t* live_toggle = nullptr;
    lv_timer_t* live_timer = nullptr;
    EditorHandler on_change = nullptr;
    bool live = true;
    EditorHandler on_accept = nullptr;
    void* accept_context = nullptr;
};

TextEditor* make_text_editor();
// `on_change` (opcjonalny) dostaje tekst w trakcie pisania, gdy włączono tryb
// „Na żywo”. `on_accept` i tak dostaje wartość końcową po „Zapisz”.
void open_text_editor(TextEditor& editor, const char* caption, const char* title,
                      const char* value, EditorMode mode, EditorHandler on_accept, void* context,
                      EditorHandler on_change = nullptr);
void close_text_editor(TextEditor& editor);

// ---------- Narzędzia ----------
void set_status_outline(lv_obj_t* obj, domain::ControlStatus status);
void flash(lv_obj_t* obj);

}  // namespace ui::widgets
