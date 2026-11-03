#include <cstring>

#include "ui_internal.hpp"

// Klimatyzacja: kafelek na ekranie Dom i pełnoekranowa podstrona. Układ według
// makiety docs/ux/mockup-pulpit (sekcja „Ekran Dom i klimatyzacja” w README).

namespace ui {
namespace {

constexpr int32_t kTileHeight = 168;
constexpr int32_t kOptionHeight = 88;

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
    const domain::OptionList* list;
    const domain::Label* current;
    const char* title;
    bool fan;  // nazwy z tabeli biegów, w przeciwnym razie żaluzji
};

OptionData option_data(const domain::ClimateState& climate, domain::ControlId control) {
    switch (control) {
        case domain::ControlId::ClimateFan:
            return {&climate.fan_modes, &climate.fan_mode, "Moc nawiewu", true};
        case domain::ControlId::ClimateSwing:
            return {&climate.swing_modes, &climate.swing_mode, "Żaluzje — pion", false};
        default:
            return {&climate.swing_horizontal_modes, &climate.swing_horizontal_mode,
                    "Żaluzje — poziom", false};
    }
}

const char* option_name(const OptionData& data, const char* id) {
    return data.fan ? name_of(kFanNames, id) : name_of(kSwingNames, id);
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
    lv_obj_add_style(option.root, &theme::styles().pressed_soft, LV_STATE_PRESSED);
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

    lv_obj_t* fill = widgets::make_fill(body);
    screen.thermo = widgets::make_thermostat(fill, &on_target, &context);

    // Tryby bez ikony wiodącej: pięć segmentów potrzebuje całej szerokości.
    screen.modes = widgets::make_segmented(body, "", domain::kMaxOptions, &on_mode, &context);
    lv_obj_add_flag(lv_obj_get_child(screen.modes.root, 0), LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* options = widgets::make_duo(body);
    lv_obj_set_height(options, kOptionHeight);
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

    screen.picker = widgets::make_page("", &on_picker_back, &context);
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

void render_tile(Context& context) {
    ClimateScreen& screen = context.climate;
    const domain::AppState& state = context.controller->state();
    const domain::ClimateState& climate = state.snapshot().climate;
    const bool fresh = state.climate_fresh();
    const bool on = fresh && climate.is_on();

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
        widgets::format_tenths(text, sizeof(text), shown_target(state));
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

void build_climate_tile(Context& context, lv_obj_t* parent) {
    ClimateScreen& screen = context.climate;

    screen.tile = lv_button_create(parent);
    lv_obj_remove_style_all(screen.tile);
    lv_obj_add_style(screen.tile, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_add_style(screen.tile, &theme::styles().pressed_soft, LV_STATE_PRESSED);
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
    lv_obj_set_style_max_width(screen.tile_fan, 230, LV_PART_MAIN);
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
    if (context.climate.tile == nullptr) {
        return;
    }
    render_tile(context);
    if (context.climate.built) {
        render_page(context);
    }
}

}  // namespace ui
