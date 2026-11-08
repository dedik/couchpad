#include "core/clock.hpp"
#include "core/config.hpp"
#include "domain/appletv_apps.hpp"
#include "images.h"
#include "ui_internal.hpp"

namespace ui {
namespace {

// Gładzik na całą szerokość: 240 px w układzie z biblioteką było za małe pod palec.
constexpr int32_t kTrackpadSize = 400;
// Kafelek aplikacji jak skrót kanału TV: pięć w rzędzie z odstępem 8 px na 432 px.
constexpr int32_t kAppTileW = (theme::kContentW - 4 * theme::kS2) / 5;  // 80
constexpr int32_t kAppTileH = kAppTileW;           // kwadrat, jak skrót kanału

// Kolejność jak w domain::kAppleTvTiles — stamtąd kafelek bierze bundle id.
const widgets::TileSpec kApps[domain::kAppleTvTileCount] = {
    {&logo_infuse, nullptr, nullptr},
    {&logo_youtube, nullptr, nullptr},
    {&logo_wppilot, nullptr, nullptr},
};


void on_app(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    const domain::AppleTvTile& tile = domain::kAppleTvTiles[index];
    const char* app_id = domain::tile_app_id(tile);
    // Aplikacji spoza świeżego snapshotu nie uruchamiamy: bridge i tak by ją odrzucił.
    if (!context.controller->state().appletv_fresh() ||
        !context.controller->state().snapshot().appletv.has_app(app_id)) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::AtvApp);
    intent.text.assign(tile.key);       // do podświetlenia oczekującego kafelka
    intent.payload.assign(app_id);      // do kontraktu
    dispatch(context, intent);
}

void send_key(Context& context, domain::NavKey key, domain::Press press) {
    const bool step = key == domain::NavKey::Up || key == domain::NavKey::Down ||
                      key == domain::NavKey::Left || key == domain::NavKey::Right;
    if (step) {
        const int64_t now = core::now_ms();
        if (now - context.appletv.last_step_ms < core::config::kTrackpadStepIntervalMs) {
            return;  // nadmiarowy krok przepada, zamiast czekać w kolejce
        }
        context.appletv.last_step_ms = now;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::Navigate);
    intent.key = key;
    intent.press = press;
    intent.target.assign("appletv");
    dispatch(context, intent);
}

void on_key(void* raw, domain::NavKey key) {
    send_key(*static_cast<Context*>(raw), key, domain::Press::Tap);
}

// Przytrzymanie OK — menu kontekstowe, jak na pilocie Siri.
void on_key_hold(void* raw, domain::NavKey key) {
    send_key(*static_cast<Context*>(raw), key, domain::Press::Hold);
}

void on_menu(void* raw) { on_key(raw, domain::NavKey::Menu); }

// Home jak na pilocie Siri: stuknięcie — ekran główny, ×2 — przełącznik
// aplikacji, przytrzymanie — Centrum sterowania.
void on_home(void* raw, domain::Press press) {
    send_key(*static_cast<Context*>(raw), domain::NavKey::Home, press);
}

void send_keyboard_text(Context& context, const char* value) {
    // Pusty tekst czyści pole. HA nie zwraca bieżącej treści, więc zawsze
    // wysyłamy całość, a nie dopisujemy znaków (ADR 0007).
    domain::UiIntent intent = make_intent(domain::IntentKind::Momentary, domain::ControlId::AtvKeyboard);
    intent.payload.assign(value);
    dispatch(context, intent);
}

void on_keyboard_text(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.appletv.keyboard_open = false;
    context.appletv.keyboard_manual = false;
    send_keyboard_text(context, value);
}

// Tryb „Na żywo”: tekst pojawia się na Apple TV w trakcie pisania.
void on_keyboard_live(void* raw, const char* value) {
    send_keyboard_text(*static_cast<Context*>(raw), value);
}

// Klawiatura idzie za stanem Apple TV: wysuwa się, gdy pojawi się pole tekstowe,
// i chowa, gdy zniknie. Nie wysuwa się nad innym ekranem.
void open_keyboard(Context& context) {
    context.appletv.keyboard_open = true;
    widgets::open_text_editor(*context.editor, "APPLE TV", "Wpisz tekst", "",
                              widgets::EditorMode::Text, &on_keyboard_text, &context,
                              &on_keyboard_live);
}

// Przycisk „Klawiatura”: otwiera ją bez czekania na pole tekstowe na TV.
void on_keyboard_button(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    if (context.editor->on_accept == &on_keyboard_text &&
        !lv_obj_has_flag(context.editor->layer, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    context.appletv.keyboard_manual = true;
    open_keyboard(context);
}

void sync_keyboard(Context& context) {
    AppleTvScreen& screen = context.appletv;
    const domain::AppState& state = context.controller->state();
    const bool active = state.appletv_fresh() && state.snapshot().appletv.keyboard_active;
    // Wysuwanie samo z siebie można wyłączyć w „Klawiatura i gładzik”; przycisk działa zawsze.
    const bool auto_open = storage::enabled(storage::Pref::KeyboardAutoOpen);

    // Ręcznie otwarta klawiatura żyje, dopóki ktoś jej nie zamknie.
    if (screen.keyboard_manual) {
        if (lv_obj_has_flag(context.editor->layer, LV_OBJ_FLAG_HIDDEN)) {
            screen.keyboard_manual = false;
            screen.keyboard_open = false;
        }
        screen.keyboard_seen = active;
        return;
    }

    if (active && auto_open && !screen.keyboard_seen && !screen.keyboard_open &&
        context.screen == ScreenId::AppleTv) {
        open_keyboard(context);
    } else if (!active && screen.keyboard_open) {
        screen.keyboard_open = false;
        // Edytor jest wspólny: zamykamy go tylko wtedy, gdy wciąż należy do nas.
        if (context.editor->on_accept == &on_keyboard_text) {
            widgets::close_text_editor(*context.editor);
        }
    }
    screen.keyboard_seen = active;
}

}  // namespace

namespace {

// Poziom (makieta docs/ux/mockup-landscape, bez karty odtwarzania): po lewej aplikacje
// i trzy akcje jedna pod drugą, po prawej gładzik. Treść 672 × 383 px:
// kolumna 260 + odstęp 16 + gładzik w 396 px.
constexpr int32_t kWideColumn = 260;
constexpr int32_t kWideAppTileH = 80;
constexpr int32_t kWideTrackpadSize = 360;

void build_appletv_landscape(Context& context) {
    AppleTvScreen& screen = context.appletv;
    widgets::make_split(screen.section, theme::kS4);
    lv_obj_t* column = widgets::make_pane(screen.section, kWideColumn, theme::kS2);

    constexpr auto kAppCount = static_cast<int32_t>(domain::kAppleTvTileCount);
    screen.apps = widgets::make_tiles(column, kApps, domain::kAppleTvTileCount, kAppCount,
                                      kWideAppTileH, &on_app, &context);

    widgets::make_spacer(column);

    // Trzy akcje w kolumnie: obok siebie w 260 px podpisy ledwo się mieściły.
    widgets::make_action(widgets::make_duo(column), LV_SYMBOL_LEFT, "Menu", false,
                         widgets::ActionLayout::Fill, &on_menu, &context);
    lv_obj_t* home = widgets::make_action(widgets::make_duo(column), LV_SYMBOL_HOME, "Home", false,
                                          widgets::ActionLayout::Fill, nullptr, nullptr);
    widgets::attach_presses(home, &on_home, &context);
    widgets::make_action(widgets::make_duo(column), LV_SYMBOL_KEYBOARD, "Tekst", false,
                         widgets::ActionLayout::Fill, &on_keyboard_button, &context);

    lv_obj_t* fill = widgets::make_fill(screen.section);
    lv_obj_set_height(fill, LV_PCT(100));
    widgets::make_trackpad(fill, &on_key, &context, &on_key_hold, kWideTrackpadSize);
}

}  // namespace

void build_appletv(Context& context, lv_obj_t* parent) {
    AppleTvScreen& screen = context.appletv;
    screen.section = widgets::make_section(parent);

    screen.banner = widgets::make_banner(screen.section,
                                         "Brak świeżego stanu Apple TV.\n"
                                         "Sterowanie wróci po nowym snapshocie.");

    if (theme::landscape()) {
        build_appletv_landscape(context);
        return;
    }

    // Aplikacje nad gładzikiem, w rozmiarze skrótów kanałów TV (pięć na szerokość):
    // trzy kafelki wyśrodkowane, nie rozciągnięte na cały rząd.
    lv_obj_t* apps_row = widgets::make_fill(screen.section);
    lv_obj_set_flex_grow(apps_row, 0);
    lv_obj_set_height(apps_row, kAppTileH);
    constexpr auto kAppCount = static_cast<int32_t>(domain::kAppleTvTileCount);
    screen.apps = widgets::make_tiles(apps_row, kApps, domain::kAppleTvTileCount, kAppCount,
                                      kAppTileH, &on_app, &context);
    lv_obj_set_width(screen.apps.root, kAppCount * kAppTileW + (kAppCount - 1) * theme::kS2);

    lv_obj_t* fill = widgets::make_fill(screen.section);
    widgets::make_trackpad(fill, &on_key, &context, &on_key_hold, kTrackpadSize);

    lv_obj_t* duo = widgets::make_duo(screen.section);
    widgets::make_action(duo, LV_SYMBOL_LEFT, "Menu", false, widgets::ActionLayout::Fill,
                         &on_menu, &context);
    lv_obj_t* home = widgets::make_action(duo, LV_SYMBOL_HOME, "Home", false,
                                          widgets::ActionLayout::Fill, nullptr, nullptr);
    widgets::attach_presses(home, &on_home, &context);
    widgets::make_action(duo, LV_SYMBOL_KEYBOARD, "Tekst", false, widgets::ActionLayout::Fill,
                         &on_keyboard_button, &context);

}

void render_appletv(Context& context) {
    AppleTvScreen& screen = context.appletv;
    const domain::AppState& state = context.controller->state();

    const domain::AppleTvState& atv = state.snapshot().appletv;
    std::size_t selected = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < domain::kAppleTvTileCount; ++i) {
        const char* app_id = domain::tile_app_id(domain::kAppleTvTiles[i]);
        if (!atv.app_id.empty() && atv.app_id == app_id) {
            selected = i;
        }
        // Kafelek bez aplikacji w snapshocie jest wygaszony: aplikacji nie ma
        // na Apple TV albo jej bundle id nie został jeszcze odczytany.
        const bool known = state.appletv_fresh() && atv.has_app(app_id);
        lv_obj_set_style_opa(screen.apps.items[i], known ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
    }
    screen.apps.set_selected(state.appletv_fresh() ? selected : static_cast<std::size_t>(-1));

    std::size_t pending = static_cast<std::size_t>(-1);
    if (const domain::PendingEntry* entry = state.pending_of(domain::ControlId::AtvApp)) {
        for (std::size_t i = 0; i < domain::kAppleTvTileCount; ++i) {
            if (entry->requested == domain::kAppleTvTiles[i].key) {
                pending = i;
            }
        }
    }
    screen.apps.set_status(pending, state.status_of(domain::ControlId::AtvApp));

    widgets::set_unavailable(screen.section, screen.banner, !state.appletv_fresh());
    sync_keyboard(context);
}

}  // namespace ui
