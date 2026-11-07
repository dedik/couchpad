#include <cstring>
#include <initializer_list>

#include "ui_internal.hpp"

// Klimatyzacja: kafelek na ekranie Dom i pełnoekranowa podstrona. Układ według
// makiety docs/ux/mockup-pulpit (sekcja „Ekran Dom i klimatyzacja” w README).

namespace ui {
namespace {

constexpr int32_t kTileHeight = 168;
constexpr int32_t kOptionHeight = 88;
// Poziom: kafel trybu i pole nawiewu na podstronie (wariant P1).
constexpr int32_t kWideModeHeight = 72;
// Siatka trybów: wybrany tryb to duży kafel w pierwszej kolumnie na wszystkie rzędy,
// pozostałe po dwa w rzędzie obok. Rzędów 2–4: encja ma najwyżej osiem trybów bez `off`.
const int32_t kModeCols[] = {LV_GRID_FR(6), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_TEMPLATE_LAST};
const int32_t kModeRows2[] = {kWideModeHeight, kWideModeHeight, LV_GRID_TEMPLATE_LAST};
const int32_t kModeRows3[] = {kWideModeHeight, kWideModeHeight, kWideModeHeight,
                              LV_GRID_TEMPLATE_LAST};
const int32_t kModeRows4[] = {kWideModeHeight, kWideModeHeight, kWideModeHeight, kWideModeHeight,
                              LV_GRID_TEMPLATE_LAST};
constexpr int32_t kWideOptionHeight = 68;

// Encja podaje identyfikatory HA. Nazwy do pokazania dobiera ekran; nieznany
// identyfikator pokazujemy tak, jak przyszedł, zamiast go ukrywać.
struct Name {
    const char* id;
    const char* label;
};

const Name kModeNames[] = {
    {"off", "Wyłączona"},  {"heat_cool", "Auto"},     {"auto", "Auto"},
    {"cool", "Chłodzenie"}, {"heat", "Grzanie"},     {"dry", "Osuszanie"},
    {"fan_only", "Wentylator"},
};

// Podpis nad temperaturą na tarczy, wersalikami jak każda etykieta.
// Ikona kafla trybu (poziom), jak w makiecie.
const Name kModeIcons[] = {
    {"heat_cool", theme::kIconAuto}, {"auto", theme::kIconAuto}, {"cool", theme::kIconCool},
    {"heat", theme::kIconHeat},      {"dry", theme::kIconDry},   {"fan_only", theme::kIconFan},
};

const Name kModeActions[] = {
    {"off", "WYŁĄCZONA"},  {"heat_cool", "AUTO DO"},      {"auto", "AUTO DO"},
    {"cool", "CHŁODZENIE DO"}, {"heat", "GRZANIE DO"},    {"dry", "OSUSZANIE DO"},
    {"fan_only", "SAM NAWIEW"},
};

// Biegi `fan_modes`: standardowe z HA i te, które podaje integracja Midea.
const Name kFanNames[] = {
    {"auto", "Auto"},        {"quiet", "Cicha"},           {"low", "Niska"},
    {"medlow", "Średnio niska"}, {"medium", "Średnia"},   {"middle", "Średnia"},
    {"medhigh", "Średnio wysoka"}, {"high", "Wysoka"},    {"strong", "Maksymalna"},
    {"top", "Maksymalna"},   {"on", "Włączona"},          {"off", "Wyłączona"},
};

const Name kSwingNames[] = {
    {"off", "Stałe"},  {"on", "Ruch"},      {"Full", "Ruch"},     {"both", "Oba kierunki"},
    {"vertical", "Pion"}, {"horizontal", "Poziom"}, {"Upper", "Góra"}, {"Lower", "Dół"},
    {"Left", "Lewo"},  {"Center", "Środek"}, {"Right", "Prawo"},
};

template <std::size_t N>
const char* name_of(const Name (&names)[N], const char* id) {
    for (const Name& name : names) {
        if (std::strcmp(name.id, id) == 0) {
            return name.label;
        }
    }
    return id;
}

// Lista, bieżąca wartość i nazwy dla sterowania, które obsługuje lista wyboru.
struct OptionData {
    enum class Names : uint8_t { Fan, Swing, Mode };
    const domain::OptionList* list;
    const domain::Label* current;
    const char* title;
    Names names;
};

OptionData option_data(const domain::ClimateState& climate, domain::ControlId control) {
    switch (control) {
        case domain::ControlId::ClimateFan:
            return {&climate.fan_modes, &climate.fan_mode, "Moc nawiewu", OptionData::Names::Fan};
        case domain::ControlId::ClimateSwing:
            return {&climate.swing_modes, &climate.swing_mode, "Żaluzje — pion",
                    OptionData::Names::Swing};
        case domain::ControlId::ClimateMode:
            // Lista z encji razem z `off`: wybór „Wyłączona” to to samo co zasilanie.
            return {&climate.hvac_modes, &climate.hvac_mode, "Tryb", OptionData::Names::Mode};
        default:
            return {&climate.swing_horizontal_modes, &climate.swing_horizontal_mode,
                    "Żaluzje — poziom", OptionData::Names::Swing};
    }
}

const char* option_name(const OptionData& data, const char* id) {
    switch (data.names) {
        case OptionData::Names::Fan:
            return name_of(kFanNames, id);
        case OptionData::Names::Mode:
            return name_of(kModeNames, id);
        default:
            return name_of(kSwingNames, id);
    }
}

lv_obj_t* box(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    theme::make_flat(obj);
    // Dzieci kafelka nie łykają dotknięcia — ma reagować na całej powierzchni.
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t* text_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_label_set_text(label, "");
    return label;
}

// ---------- zamiary ----------

void on_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    const domain::AppState& state = context.controller->state();
    if (!state.climate_fresh()) {
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetPower;
    intent.control = domain::ControlId::ClimatePower;
    // Włączenie wraca do ostatniego trybu, jak `climate.turn_on` w HA.
    intent.flag = !state.snapshot().climate.is_on();
    dispatch(context, intent);
}

void on_mode(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= context.climate.mode_count) {
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetOption;
    intent.control = domain::ControlId::ClimateMode;
    intent.text = context.climate.mode_ids[index];
    dispatch(context, intent);
}

void on_target(void* raw, int32_t tenths, bool) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetValue;
    intent.control = domain::ControlId::ClimateTarget;
    intent.number = static_cast<int16_t>(tenths);
    dispatch(context, intent);
}

void open_picker(Context& context, domain::ControlId control) {
    const OptionData data = option_data(context.controller->state().snapshot().climate, control);
    if (data.list->empty()) {
        return;
    }
    context.climate.picker_control = control;
    lv_label_set_text(context.climate.picker.title, data.title);
    widgets::show_page(context.climate.picker, true);
    render(context);
}

void on_picker_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    context.climate.picker_control = domain::ControlId::None;
    widgets::show_page(context.climate.picker, false);
}

void on_picker_row(void* raw) {
    auto& row = *static_cast<ClimateScreen::PickerRow*>(raw);
    Context& context = *row.context;
    const domain::ControlId control = context.climate.picker_control;
    const OptionData data = option_data(context.controller->state().snapshot().climate, control);
    if (row.index >= data.list->size()) {
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetOption;
    intent.control = control;
    intent.text = (*data.list)[row.index];
    // Lista znika od razu; oczekiwanie widać na kafelku opcji.
    on_picker_back(&context);
    dispatch(context, intent);
}

void on_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    on_picker_back(&context);
    widgets::show_page(context.climate.page, false);
}

// ---------- budowa podstrony ----------

ClimateScreen::Option make_option(lv_obj_t* row, const char* caption, lv_event_cb_t on_press,
                                  Context& context) {
    ClimateScreen::Option option;
    option.root = lv_button_create(row);
    lv_obj_remove_style_all(option.root);
    lv_obj_add_style(option.root, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_height(option.root, kOptionHeight);
    lv_obj_set_width(option.root, 0);
    lv_obj_set_flex_grow(option.root, 1);
    lv_obj_set_style_pad_hor(option.root, theme::kS4, LV_PART_MAIN);
    theme::make_column(option.root, 6);
    lv_obj_set_flex_align(option.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    widgets::make_cap(option.root, caption);
    option.value = text_label(option.root, theme::font_m15(), theme::text());
    lv_obj_set_width(option.value, LV_PCT(100));
    lv_label_set_long_mode(option.value, LV_LABEL_LONG_DOT);

    lv_obj_add_event_cb(option.root, on_press, LV_EVENT_CLICKED, &context);
    return option;
}

void build_page(Context& context) {
    ClimateScreen& screen = context.climate;
    screen.page = widgets::make_page("Klimatyzacja", &on_back, &context);
    // Zasilanie po drugiej stronie nagłówka niż Cofnij, jak na ekranach urządzeń.
    screen.power = widgets::make_power_button(lv_obj_get_parent(screen.page.title), &on_power,
                                              &context);

    lv_obj_t* body = screen.page.body;
    screen.banner = widgets::make_banner(body,
                                         "Brak świeżego stanu klimatyzacji.\n"
                                         "Sterowanie wróci po nowym snapshocie.");

    const bool wide = theme::landscape();
    lv_obj_t* fill = widgets::make_fill(body);
    screen.thermo = widgets::make_thermostat(fill, &on_target, &context);

    // Poziom (makieta docs/ux/mockup-landscape, wariant P1): tarcza po lewej na pełną
    // wysokość, po prawej tryby jako kafle 3 × 2 i pod nimi nawiew w jednym rzędzie.
    // Prawa kolumna zaczyna się 16 px niżej, żeby zasilanie w nagłówku nie stykało się
    // z pierwszym kafelkiem.
    lv_obj_t* options = nullptr;
    lv_obj_t* side = nullptr;
    if (wide) {
        lv_obj_set_flex_align(fill, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(fill, theme::kS5, LV_PART_MAIN);
        lv_obj_set_width(screen.thermo->root, 320);
        side = lv_obj_create(fill);
        theme::make_flat(side);
        lv_obj_set_height(side, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(side, 1);
        theme::make_column(side, theme::kS2);
        lv_obj_set_style_pad_top(side, theme::kS4, LV_PART_MAIN);
        widgets::make_cap(side, "TRYB");
    }

    // Tryby bez ikony wiodącej: pięć segmentów potrzebuje całej szerokości.
    screen.modes = widgets::make_segmented(wide ? side : body, "", domain::kMaxOptions, &on_mode,
                                           &context);
    lv_obj_add_flag(lv_obj_get_child(screen.modes.root, 0), LV_OBJ_FLAG_HIDDEN);

    if (wide) {
        // Osobne kafle po trzy w rzędzie, z ikoną nad nazwą — jak w makiecie (P1):
        // bez wspólnego tła grupy, wybrany ma miękkie bursztynowe tło i obwódkę.
        lv_obj_t* grid = screen.modes.box;
        lv_obj_set_width(grid, LV_PCT(100));
        lv_obj_set_flex_grow(grid, 0);
        lv_obj_set_layout(grid, LV_LAYOUT_GRID);
        lv_obj_set_grid_dsc_array(grid, kModeCols, kModeRows2);
        lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_column(grid, theme::kS2, LV_PART_MAIN);
        lv_obj_set_style_pad_row(grid, theme::kS2, LV_PART_MAIN);
        lv_obj_set_height(grid, LV_SIZE_CONTENT);
        for (std::size_t i = 0; i < screen.modes.count; ++i) {
            lv_obj_t* item = screen.modes.items[i];
            lv_obj_set_flex_grow(item, 0);
            lv_obj_set_style_radius(item, theme::kR2, LV_PART_MAIN);
            // Napis jest dzieckiem 0 (pisze go set_labels), ikona dochodzi jako 1;
            // odwrócona kolumna stawia ją nad napisem.
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN_REVERSE);
            lv_obj_set_style_pad_row(item, 6, LV_PART_MAIN);
            lv_obj_t* label = lv_obj_get_child(item, 0);
            lv_obj_set_style_text_font(label, theme::font_r13(), LV_PART_MAIN);
            screen.mode_icons[i] = text_label(item, theme::font_mode_icon(), theme::dim());
        }
        lv_obj_t* cap = widgets::make_cap(side, "NAWIEW");
        lv_obj_set_style_margin_top(cap, theme::kS2, LV_PART_MAIN);
        options = widgets::make_duo(side);
        lv_obj_set_height(options, kWideOptionHeight);
    } else {
        options = widgets::make_duo(body);
        lv_obj_set_height(options, kOptionHeight);
    }
    screen.fan = make_option(
        options, "MOC",
        [](lv_event_t* event) {
            open_picker(*static_cast<Context*>(lv_event_get_user_data(event)),
                        domain::ControlId::ClimateFan);
        },
        context);
    screen.swing = make_option(
        options, "PION",
        [](lv_event_t* event) {
            open_picker(*static_cast<Context*>(lv_event_get_user_data(event)),
                        domain::ControlId::ClimateSwing);
        },
        context);
    screen.swing_h = make_option(
        options, "POZIOM",
        [](lv_event_t* event) {
            open_picker(*static_cast<Context*>(lv_event_get_user_data(event)),
                        domain::ControlId::ClimateSwingH);
        },
        context);
    if (wide) {
        for (lv_obj_t* card : {screen.fan.root, screen.swing.root, screen.swing_h.root}) {
            lv_obj_set_height(card, kWideOptionHeight);
            // Jak w makiecie: pole bez obwódki, na tle surface-2 (obwódkę dostaje
            // tylko stan oczekiwania lub błędu — set_status_outline).
            lv_obj_set_style_bg_color(card, theme::surface2(), LV_PART_MAIN);
            lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(card, theme::kR2, LV_PART_MAIN);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
        // Wartość pod podpisem, na środku pola jak on.
        for (lv_obj_t* value : {screen.fan.value, screen.swing.value, screen.swing_h.value}) {
            lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        }
    }

    screen.picker = widgets::make_page("", &on_picker_back, &context);
    widgets::dock_sheet(screen.picker);
    for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
        ClimateScreen::PickerRow& row = screen.picker_rows[i];
        row.context = &context;
        row.index = i;
        row.row = widgets::make_list_row(screen.picker.body, "", "", "", "", &on_picker_row,
                                         &row);
        // Wiersz wyboru, nie przejście dalej: bez ikony, strzałki i opisu.
        lv_obj_add_flag(lv_obj_get_child(row.row.root, 0), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_obj_get_child(row.row.root, -1), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(row.row.note, LV_OBJ_FLAG_HIDDEN);
    }

    screen.built = true;
}

void on_tile(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    if (!context.climate.built) {
        build_page(context);
    }
    widgets::show_page(context.climate.page, true);
    render(context);
}

// ---------- rysowanie ----------

// Temperatura docelowa w toku, potem potwierdzona.
int32_t shown_target(const domain::AppState& state) {
    if (const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ClimateTarget)) {
        return pending->requested_number;
    }
    return state.snapshot().climate.target_tenths;
}

// ---------- kafelek w poziomie: −/+ i tryb ----------

constexpr uint32_t kTileCommitMs = 700;  // jak core::config dla tarczy temperatury

void tile_commit(lv_timer_t* timer) {
    auto& context = *static_cast<Context*>(lv_timer_get_user_data(timer));
    // Licznik powtórzeń 1: LVGL usuwa zegar sam po tym wywołaniu.
    context.climate.tile_timer = nullptr;
    on_target(&context, context.climate.tile_pending, false);
}

void tile_step(Context& context, int32_t direction) {
    const domain::AppState& state = context.controller->state();
    const domain::ClimateState& climate = state.snapshot().climate;
    if (!state.climate_fresh() || !climate.is_on() || !climate.uses_target()) {
        return;
    }
    ClimateScreen& screen = context.climate;
    const int32_t step = climate.step_tenths > 0 ? climate.step_tenths : 5;
    const int32_t from = screen.tile_timer != nullptr ? screen.tile_pending : shown_target(state);
    int32_t next = from + direction * step;
    next = next < climate.min_tenths ? climate.min_tenths : next;
    next = next > climate.max_tenths ? climate.max_tenths : next;
    screen.tile_pending = next;
    if (screen.tile_timer == nullptr) {
        screen.tile_timer = lv_timer_create(tile_commit, kTileCommitMs, &context);
        lv_timer_set_repeat_count(screen.tile_timer, 1);
    } else {
        lv_timer_reset(screen.tile_timer);
    }
    render(context);
}

void on_tile_mode(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    if (!context.controller->state().climate_fresh()) {
        return;
    }
    // Lista wyboru należy do podstrony, która powstaje dopiero przy pierwszym otwarciu.
    if (!context.climate.built) {
        build_page(context);
    }
    open_picker(context, domain::ControlId::ClimateMode);
}

void render_tile(Context& context) {
    ClimateScreen& screen = context.climate;
    const domain::AppState& state = context.controller->state();
    const domain::ClimateState& climate = state.snapshot().climate;
    const bool fresh = state.climate_fresh();
    const bool on = fresh && climate.is_on();

    if (screen.tile_power != nullptr) {
        widgets::set_power_state(screen.tile_power, on, fresh);
        widgets::set_status_outline(screen.tile_power,
                                    state.status_of(domain::ControlId::ClimatePower));
        lv_obj_set_style_bg_opa(screen.tile_pill, on ? theme::kAccentSoftOpa : static_cast<lv_opa_t>(LV_OPA_COVER),
                                LV_PART_MAIN);
        lv_obj_set_style_bg_color(screen.tile_pill, on ? theme::accent() : theme::surface2(),
                                  LV_PART_MAIN);
        widgets::set_status_outline(screen.tile_pill,
                                    state.status_of(domain::ControlId::ClimateMode));
        const bool steps = on && climate.uses_target();
        lv_obj_set_style_opa(screen.tile_steps, steps ? LV_OPA_COVER : 77, LV_PART_MAIN);
    }

    char text[32];
    // Sam nawiew: temperatura docelowa nic nie znaczy, a moc nawiewu — tak.
    const bool fan_only = fresh && climate.hvac_mode == "fan_only";
    if (fan_only) {
        const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ClimateFan);
        const char* id = pending != nullptr ? pending->requested.c_str() : climate.fan_mode.c_str();
        lv_label_set_text(screen.tile_fan, *id != '\0' ? name_of(kFanNames, id) : "—");
        lv_obj_add_flag(screen.tile_temp_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(screen.tile_fan_box, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen.tile_fan_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(screen.tile_temp_box, LV_OBJ_FLAG_HIDDEN);
    }
    if (fresh && climate.uses_target()) {
        // Seria stuknięć w −/+ na kafelku: liczba idzie za palcem, zanim wyjdzie polecenie.
        widgets::format_tenths(text, sizeof(text), screen.tile_timer != nullptr
                                                       ? screen.tile_pending
                                                       : shown_target(state));
        lv_label_set_text(screen.tile_temp, text);
        lv_obj_remove_flag(screen.tile_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(screen.tile_temp, "—");
        lv_obj_add_flag(screen.tile_unit, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_text_color(screen.tile_temp, on ? theme::text() : theme::dim(), LV_PART_MAIN);

    lv_label_set_text(screen.tile_mode, fresh ? name_of(kModeNames, climate.hvac_mode.c_str())
                                              : "Brak świeżego stanu");
    lv_obj_set_style_text_color(screen.tile_mode, on ? theme::accent() : theme::dim(), LV_PART_MAIN);

    if (!fresh) {
        lv_label_set_text(screen.tile_now, "czekam na snapshot");
    } else if (climate.has_current) {
        widgets::format_tenths(text, sizeof(text), climate.current_tenths);
        lv_label_set_text_fmt(screen.tile_now, "W pokoju %s°", text);
    } else {
        lv_label_set_text(screen.tile_now, "");
    }

    lv_obj_set_style_bg_color(screen.tile_dot,
                              !fresh ? theme::danger() : on ? theme::accent() : theme::faint(),
                              LV_PART_MAIN);
    lv_obj_set_style_border_color(screen.tile, fresh ? theme::line() : theme::danger(),
                                  LV_PART_MAIN);
}

void render_page(Context& context) {
    ClimateScreen& screen = context.climate;
    const domain::AppState& state = context.controller->state();
    const domain::ClimateState& climate = state.snapshot().climate;
    const bool fresh = state.climate_fresh();

    widgets::set_power_state(screen.power, fresh && climate.is_on(), fresh);
    widgets::set_status_outline(screen.power, state.status_of(domain::ControlId::ClimatePower));

    widgets::Thermostat& thermo = *screen.thermo;
    if (fresh) {
        thermo.set_range(climate.min_tenths, climate.max_tenths, climate.step_tenths);
    }
    thermo.set_enabled(fresh && climate.uses_target());
    // Pod palcem i przed wysłaniem serii stuknięć wartość należy do tarczy.
    if (!thermo.busy()) {
        thermo.set_value(shown_target(state));
        thermo.set_status(state.status_of(domain::ControlId::ClimateTarget));
    }
    char now[32] = "";
    if (fresh && climate.has_current) {
        char text[16];
        widgets::format_tenths(text, sizeof(text), climate.current_tenths);
        lv_snprintf(now, sizeof(now), "%s° w pokoju", text);
    }
    thermo.set_texts(fresh ? name_of(kModeActions, climate.hvac_mode.c_str()) : "", now);

    // Tryby z encji, bez `off`.
    const char* labels[domain::kMaxOptions]{};
    screen.mode_count = 0;
    std::size_t selected = domain::kMaxOptions;
    std::size_t requested = domain::kMaxOptions;
    const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ClimateMode);
    for (std::size_t i = 0; i < climate.hvac_modes.size(); ++i) {
        const domain::Label& id = climate.hvac_modes[i];
        if (id == "off") {
            continue;
        }
        const std::size_t slot = screen.mode_count++;
        screen.mode_ids[slot] = id;
        labels[slot] = name_of(kModeNames, id.c_str());
        if (id == climate.hvac_mode) {
            selected = slot;
        }
        if (pending != nullptr && id == pending->requested) {
            requested = slot;
        }
    }
    screen.modes.set_labels(labels, screen.mode_count);
    if (fresh && selected < screen.mode_count) {
        screen.modes.set_selected(selected);
    } else {
        screen.modes.set_selected_none();
    }
    screen.modes.set_status(requested, state.status_of(domain::ControlId::ClimateMode));
    if (screen.mode_icons[0] != nullptr) {
        // Wygląd kafla z makiety zamiast podkreślenia segmentu (styl lokalny ma pierwszeństwo).
        const std::size_t chosen = fresh ? selected : domain::kMaxOptions;
        // Duży kafel: wybrany tryb, a bez świeżego stanu pierwszy z listy.
        const std::size_t big = chosen < screen.mode_count ? chosen : 0;
        const std::size_t others = screen.mode_count > 0 ? screen.mode_count - 1 : 0;
        const std::size_t rows = others <= 4 ? 2 : (others <= 6 ? 3 : 4);
        lv_obj_set_grid_dsc_array(screen.modes.box, kModeCols,
                                  rows == 2 ? kModeRows2 : rows == 3 ? kModeRows3 : kModeRows4);
        std::size_t slot = 0;
        for (std::size_t i = 0; i < screen.mode_count; ++i) {
            lv_obj_t* item = screen.modes.items[i];
            const bool large = i == big;
            if (large) {
                lv_obj_set_grid_cell(item, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0,
                                     static_cast<int32_t>(rows));
            } else {
                lv_obj_set_grid_cell(item, LV_GRID_ALIGN_STRETCH, 1 + static_cast<int32_t>(slot % 2), 1,
                                     LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(slot / 2), 1);
                ++slot;
            }
            lv_obj_set_style_text_font(lv_obj_get_child(item, 0),
                                       large ? theme::font_m15() : theme::font_r13(), LV_PART_MAIN);
            lv_obj_set_style_text_font(screen.mode_icons[i],
                                       large ? theme::font_mode_icon_lg() : theme::font_mode_icon(),
                                       LV_PART_MAIN);
            lv_obj_set_style_pad_row(item, large ? 10 : 6, LV_PART_MAIN);
            const bool on = i == chosen;
            lv_obj_set_style_bg_opa(item, on ? theme::kAccentSoftOpa : static_cast<lv_opa_t>(LV_OPA_COVER),
                                    LV_PART_MAIN);
            lv_obj_set_style_bg_color(item, on ? theme::accent() : theme::surface2(), LV_PART_MAIN);
            lv_obj_set_style_border_width(item, on ? 1 : 0, LV_PART_MAIN);
            lv_obj_set_style_border_side(item, LV_BORDER_SIDE_FULL, LV_PART_MAIN);
            lv_obj_set_style_border_color(item, theme::accent(), LV_PART_MAIN);
            const lv_color_t ink = on ? theme::accent() : theme::dim();
            lv_obj_set_style_text_color(lv_obj_get_child(item, 0), ink, LV_PART_MAIN);
            lv_obj_set_style_text_color(screen.mode_icons[i], ink, LV_PART_MAIN);
            lv_label_set_text(screen.mode_icons[i], name_of(kModeIcons, screen.mode_ids[i].c_str()));
        }
    }

    // Kafelki opcji: wybór w toku, potem bieżąca wartość.
    const struct {
        ClimateScreen::Option* option;
        domain::ControlId control;
    } options[] = {
        {&screen.fan, domain::ControlId::ClimateFan},
        {&screen.swing, domain::ControlId::ClimateSwing},
        {&screen.swing_h, domain::ControlId::ClimateSwingH},
    };
    for (const auto& item : options) {
        const OptionData data = option_data(climate, item.control);
        const domain::PendingEntry* entry = state.pending_of(item.control);
        const char* id = entry != nullptr ? entry->requested.c_str() : data.current->c_str();
        lv_label_set_text(item.option->value, fresh && *id != '\0' ? option_name(data, id) : "—");
        widgets::set_status_outline(item.option->root, state.status_of(item.control));
        // Encja bez tej opcji (np. bez żaluzji poziomych) — kafelek znika.
        if (fresh && data.list->empty()) {
            lv_obj_add_flag(item.option->root, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(item.option->root, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (screen.picker_control != domain::ControlId::None) {
        const OptionData data = option_data(climate, screen.picker_control);
        for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
            const widgets::ListRow& row = screen.picker_rows[i].row;
            if (i >= data.list->size()) {
                lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(row.root, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(row.title, option_name(data, (*data.list)[i].c_str()));
            const bool current = fresh && (*data.list)[i] == *data.current;
            lv_label_set_text(row.value, current ? "aktywne" : "");
            lv_obj_set_style_border_color(row.root, current ? theme::accent() : theme::line(),
                                          LV_PART_MAIN);
        }
    }

    widgets::set_unavailable(screen.page.body, screen.banner, !fresh);
}

}  // namespace

const char* climate_mode_name(const char* id) {
    return name_of(kModeNames, id);
}

namespace {

lv_obj_t* tile_button(lv_obj_t* parent, const char* symbol, lv_event_cb_t on_press,
                      Context& context) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &theme::styles().act, LV_PART_MAIN);
    lv_obj_set_height(button, LV_PCT(100));
    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* icon = text_label(button, theme::font_icon(), theme::text());
    lv_label_set_text(icon, symbol);
    lv_obj_add_event_cb(button, on_press, LV_EVENT_CLICKED, &context);
    return button;
}

// Poziom (makieta docs/ux/mockup-landscape, wariant C): wysoka karta obok scen.
// Nagłówek z zasilaniem, na środku tryb (dotknięcie: lista trybów) i duża temperatura,
// na dole −/+. Dotknięcie wolnego miejsca karty nadal otwiera podstronę.
void build_tile_wide(Context& context, lv_obj_t* parent) {
    ClimateScreen& screen = context.climate;

    screen.tile = lv_button_create(parent);
    lv_obj_remove_style_all(screen.tile);
    lv_obj_add_style(screen.tile, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_width(screen.tile, LV_PCT(100));
    lv_obj_set_style_radius(screen.tile, theme::kR4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.tile, theme::kS4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(screen.tile, theme::kS4, LV_PART_MAIN);
    theme::make_column(screen.tile, theme::kS3);

    lv_obj_t* head = box(screen.tile);
    // 64 px: tyle zajmuje poszerzone pole dotyku przycisku zasilania.
    lv_obj_set_size(head, LV_PCT(100), theme::kIdentityRow);
    theme::make_row(head, theme::kS3);
    screen.tile_dot = box(head);
    lv_obj_set_size(screen.tile_dot, 10, 10);
    lv_obj_set_style_radius(screen.tile_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.tile_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* names = box(head);
    lv_obj_set_height(names, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(names, 1);
    theme::make_column(names, 3);
    lv_obj_t* title = text_label(names, theme::font_m15(), theme::text());
    lv_label_set_text(title, "Klimatyzacja");
    widgets::make_cap(names, context.room.c_str());
    screen.tile_power = widgets::make_power_button(head, &on_power, &context);

    lv_obj_t* middle = box(screen.tile);
    lv_obj_set_width(middle, LV_PCT(100));
    lv_obj_set_flex_grow(middle, 1);
    theme::make_column(middle, theme::kS2);
    lv_obj_set_flex_align(middle, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    screen.tile_pill = lv_button_create(middle);
    lv_obj_remove_style_all(screen.tile_pill);
    lv_obj_set_size(screen.tile_pill, LV_SIZE_CONTENT, 44);
    lv_obj_set_style_pad_hor(screen.tile_pill, theme::kS4, LV_PART_MAIN);
    lv_obj_set_style_radius(screen.tile_pill, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.tile_pill, LV_OPA_COVER, LV_PART_MAIN);
    theme::make_row(screen.tile_pill, theme::kS2);
    screen.tile_mode = text_label(screen.tile_pill, theme::font_m15(), theme::dim());
    lv_obj_t* chevron = text_label(screen.tile_pill, theme::font_icon_xs(), theme::faint());
    lv_label_set_text(chevron, LV_SYMBOL_DOWN);
    lv_obj_add_event_cb(screen.tile_pill, on_tile_mode, LV_EVENT_CLICKED, &context);

    lv_obj_t* temp = box(middle);
    screen.tile_temp_box = temp;
    lv_obj_set_size(temp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(temp, 2, LV_PART_MAIN);
    screen.tile_temp = text_label(temp, theme::font_b64num(), theme::dim());
    screen.tile_unit = text_label(temp, theme::font_b26(), theme::faint());
    lv_label_set_text(screen.tile_unit, "°C");

    screen.tile_fan_box = box(middle);
    lv_obj_set_size(screen.tile_fan_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(screen.tile_fan_box, 4);
    lv_obj_set_flex_align(screen.tile_fan_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    widgets::make_cap(screen.tile_fan_box, "MOC NAWIEWU");
    screen.tile_fan = text_label(screen.tile_fan_box, theme::font_b26(), theme::text());
    lv_obj_add_flag(screen.tile_fan_box, LV_OBJ_FLAG_HIDDEN);

    screen.tile_now = text_label(middle, theme::font_r13(), theme::dim());

    screen.tile_steps = box(screen.tile);
    lv_obj_set_size(screen.tile_steps, LV_PCT(100), theme::kTouch);
    theme::make_row(screen.tile_steps, theme::kS2);
    tile_button(screen.tile_steps, LV_SYMBOL_MINUS,
                [](lv_event_t* event) {
                    tile_step(*static_cast<Context*>(lv_event_get_user_data(event)), -1);
                },
                context);
    tile_button(screen.tile_steps, LV_SYMBOL_PLUS,
                [](lv_event_t* event) {
                    tile_step(*static_cast<Context*>(lv_event_get_user_data(event)), 1);
                },
                context);

    lv_obj_add_event_cb(screen.tile, on_tile, LV_EVENT_CLICKED, &context);
}

}  // namespace

void build_climate_tile(Context& context, lv_obj_t* parent) {
    ClimateScreen& screen = context.climate;
    if (theme::landscape()) {
        build_tile_wide(context, parent);
        return;
    }

    screen.tile = lv_button_create(parent);
    lv_obj_remove_style_all(screen.tile);
    lv_obj_add_style(screen.tile, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_size(screen.tile, LV_PCT(100), kTileHeight);
    lv_obj_set_style_radius(screen.tile, theme::kR4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.tile, theme::kS5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(screen.tile, theme::kS4, LV_PART_MAIN);
    theme::make_column(screen.tile, 0);
    lv_obj_set_flex_align(screen.tile, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    // Nagłówek: kropka stanu, nazwa i pokój, strzałka przejścia.
    lv_obj_t* head = box(screen.tile);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_row(head, theme::kS3);

    screen.tile_dot = box(head);
    lv_obj_set_size(screen.tile_dot, 10, 10);
    lv_obj_set_style_radius(screen.tile_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.tile_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.tile_dot, theme::faint(), LV_PART_MAIN);

    lv_obj_t* names = box(head);
    lv_obj_set_height(names, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(names, 1);
    theme::make_column(names, 3);
    lv_obj_t* title = text_label(names, theme::font_m15(), theme::text());
    lv_label_set_text(title, "Klimatyzacja");
    widgets::make_cap(names, context.room.c_str());

    lv_obj_t* go = text_label(head, theme::font_icon_sm(), theme::faint());
    lv_label_set_text(go, LV_SYMBOL_RIGHT);

    // Treść: temperatura docelowa po lewej, tryb i temperatura w pokoju po prawej.
    lv_obj_t* body = box(screen.tile);
    lv_obj_set_size(body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);

    lv_obj_t* temp = box(body);
    screen.tile_temp_box = temp;
    lv_obj_set_size(temp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(temp, 2, LV_PART_MAIN);
    screen.tile_temp = text_label(temp, theme::font_b46num(), theme::dim());
    screen.tile_unit = text_label(temp, theme::font_b19(), theme::faint());
    lv_label_set_text(screen.tile_unit, "°C");

    // Zamiast temperatury, w trybie wentylatora: podpis i bieg nawiewu.
    screen.tile_fan_box = box(body);
    lv_obj_set_size(screen.tile_fan_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(screen.tile_fan_box, 4);
    widgets::make_cap(screen.tile_fan_box, "MOC NAWIEWU");
    screen.tile_fan = text_label(screen.tile_fan_box, theme::font_b26(), theme::text());
    lv_label_set_long_mode(screen.tile_fan, LV_LABEL_LONG_DOT);
    // Stała szerokość: z szerokością „wg treści” i wielokropkiem LVGL pokazywał samo „…”.
    lv_obj_set_width(screen.tile_fan, 230);
    lv_obj_add_flag(screen.tile_fan_box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* meta = box(body);
    lv_obj_set_size(meta, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(meta, 4);
    lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    screen.tile_mode = text_label(meta, theme::font_m15(), theme::dim());
    screen.tile_now = text_label(meta, theme::font_r13(), theme::faint());

    lv_obj_add_event_cb(screen.tile, on_tile, LV_EVENT_CLICKED, &context);
}

void render_climate(Context& context) {
    // Od ADR 0014 ekran Dom nie ma dużego kafelka klimy (pasek albo pigułka w screen_home.cpp),
    // ale podstrona dalej się otwiera — także z karty klimy w salonie.
    if (context.climate.tile != nullptr) {
        render_tile(context);
    }
    if (context.climate.built) {
        render_page(context);
    }
}

void open_climate_page(Context& context) {
    if (!context.climate.built) {
        build_page(context);
    }
    widgets::show_page(context.climate.page, true);
    render(context);
}

}  // namespace ui
