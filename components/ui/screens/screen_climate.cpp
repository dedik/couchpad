#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "climate_internal.hpp"

// Klimatyzacja: pełnoekranowa podstrona (tarcza, tryby, nawiew, listy wyboru). Kafelek na
// ekranie Dom jest w climate_tile.cpp. Układ według makiety docs/ux/mockup-pulpit
// (sekcja „Ekran Dom i klimatyzacja” w README).

namespace ui::climate {

namespace {

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

}  // namespace

// ---------- zamiary ----------

void on_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    const domain::AppState& state = context.controller->state();
    if (!state.climate_fresh()) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetPower, domain::ControlId::ClimatePower);
    // Włączenie wraca do ostatniego trybu, jak `climate.turn_on` w HA.
    intent.flag = !state.snapshot().climate.is_on();
    dispatch(context, intent);
}

void on_mode(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index >= context.climate.mode_count) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::ClimateMode);
    intent.text = context.climate.mode_ids[index];
    dispatch(context, intent);
}

void on_target(void* raw, int32_t tenths, bool) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent intent = make_intent(domain::IntentKind::SetValue, domain::ControlId::ClimateTarget);
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
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, control);
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
    option.root = widgets::make_bare_button(row);
    lv_obj_add_style(option.root, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_height(option.root, kOptionHeight);
    lv_obj_set_width(option.root, 0);
    lv_obj_set_flex_grow(option.root, 1);
    lv_obj_set_style_pad_hor(option.root, theme::kS4, LV_PART_MAIN);
    theme::make_column(option.root, 6);
    lv_obj_set_flex_align(option.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    widgets::make_cap(option.root, caption);
    option.value = widgets::make_text(option.root, theme::font_m15(), theme::text());
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
            screen.mode_icons[i] = widgets::make_text(item, theme::font_mode_icon(), theme::dim());
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

// Temperatura docelowa w toku, potem potwierdzona.
int32_t shown_target(const domain::AppState& state) {
    if (const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ClimateTarget)) {
        return pending->requested_number;
    }
    return state.snapshot().climate.target_tenths;
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
            lv_obj_set_style_bg_opa(item, on ? theme::kAccentSoftOpa : theme::kOpaCover,
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
        widgets::set_visible(item.option->root, !(fresh && data.list->empty()));
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

}  // namespace ui::climate

namespace ui {

using namespace climate;

const char* climate_mode_name(const char* id) {
    return name_of(kModeNames, id);
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
