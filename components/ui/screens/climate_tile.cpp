#include <cstdio>
#include <cstring>

#include "climate_internal.hpp"

// Klimatyzacja: kafelek na ekranie Dom — w pionie karta z temperaturą i trybem, w poziomie
// wariant C z −/+ i trybem (makieta docs/ux/mockup-landscape). Dotknięcie wolnego miejsca
// otwiera podstronę (screen_climate.cpp).

namespace ui::climate {

namespace {

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

lv_obj_t* tile_button(lv_obj_t* parent, const char* symbol, lv_event_cb_t on_press,
                      Context& context) {
    lv_obj_t* button = widgets::make_bare_button(parent);
    lv_obj_add_style(button, &theme::styles().act, LV_PART_MAIN);
    lv_obj_set_height(button, LV_PCT(100));
    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* icon = widgets::make_text(button, theme::font_icon(), theme::text());
    lv_label_set_text(icon, symbol);
    lv_obj_add_event_cb(button, on_press, LV_EVENT_CLICKED, &context);
    return button;
}

// Poziom (makieta docs/ux/mockup-landscape, wariant C): wysoka karta obok scen.
// Nagłówek z zasilaniem, na środku tryb (dotknięcie: lista trybów) i duża temperatura,
// na dole −/+. Dotknięcie wolnego miejsca karty nadal otwiera podstronę.
void build_tile_wide(Context& context, lv_obj_t* parent) {
    ClimateScreen& screen = context.climate;

    screen.tile = widgets::make_bare_button(parent);
    lv_obj_add_style(screen.tile, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_width(screen.tile, LV_PCT(100));
    lv_obj_set_style_radius(screen.tile, theme::kR4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.tile, theme::kS4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(screen.tile, theme::kS4, LV_PART_MAIN);
    theme::make_column(screen.tile, theme::kS3);

    lv_obj_t* head = widgets::make_box(screen.tile);
    // 64 px: tyle zajmuje poszerzone pole dotyku przycisku zasilania.
    lv_obj_set_size(head, LV_PCT(100), theme::kIdentityRow);
    theme::make_row(head, theme::kS3);
    screen.tile_dot = widgets::make_box(head);
    lv_obj_set_size(screen.tile_dot, 10, 10);
    lv_obj_set_style_radius(screen.tile_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.tile_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* names = widgets::make_box(head);
    lv_obj_set_height(names, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(names, 1);
    theme::make_column(names, 3);
    lv_obj_t* title = widgets::make_text(names, theme::font_m15(), theme::text());
    lv_label_set_text(title, "Klimatyzacja");
    widgets::make_cap(names, context.room.c_str());
    screen.tile_power = widgets::make_power_button(head, &on_power, &context);

    lv_obj_t* middle = widgets::make_box(screen.tile);
    lv_obj_set_width(middle, LV_PCT(100));
    lv_obj_set_flex_grow(middle, 1);
    theme::make_column(middle, theme::kS2);
    lv_obj_set_flex_align(middle, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    screen.tile_pill = widgets::make_bare_button(middle);
    lv_obj_set_size(screen.tile_pill, LV_SIZE_CONTENT, 44);
    lv_obj_set_style_pad_hor(screen.tile_pill, theme::kS4, LV_PART_MAIN);
    lv_obj_set_style_radius(screen.tile_pill, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.tile_pill, LV_OPA_COVER, LV_PART_MAIN);
    theme::make_row(screen.tile_pill, theme::kS2);
    screen.tile_mode = widgets::make_text(screen.tile_pill, theme::font_m15(), theme::dim());
    lv_obj_t* chevron = widgets::make_text(screen.tile_pill, theme::font_icon_xs(), theme::faint());
    lv_label_set_text(chevron, LV_SYMBOL_DOWN);
    lv_obj_add_event_cb(screen.tile_pill, on_tile_mode, LV_EVENT_CLICKED, &context);

    lv_obj_t* temp = widgets::make_box(middle);
    screen.tile_temp_box = temp;
    lv_obj_set_size(temp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(temp, 2, LV_PART_MAIN);
    screen.tile_temp = widgets::make_text(temp, theme::font_b64num(), theme::dim());
    screen.tile_unit = widgets::make_text(temp, theme::font_b26(), theme::faint());
    lv_label_set_text(screen.tile_unit, "°C");

    screen.tile_fan_box = widgets::make_box(middle);
    lv_obj_set_size(screen.tile_fan_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(screen.tile_fan_box, 4);
    lv_obj_set_flex_align(screen.tile_fan_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    widgets::make_cap(screen.tile_fan_box, "MOC NAWIEWU");
    screen.tile_fan = widgets::make_text(screen.tile_fan_box, theme::font_b26(), theme::text());
    lv_obj_add_flag(screen.tile_fan_box, LV_OBJ_FLAG_HIDDEN);

    screen.tile_now = widgets::make_text(middle, theme::font_r13(), theme::dim());

    screen.tile_steps = widgets::make_box(screen.tile);
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
        lv_obj_set_style_bg_opa(screen.tile_pill, on ? theme::kAccentSoftOpa : theme::kOpaCover,
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

}  // namespace ui::climate

namespace ui {

using namespace climate;

void build_climate_tile(Context& context, lv_obj_t* parent) {
    ClimateScreen& screen = context.climate;
    if (theme::landscape()) {
        build_tile_wide(context, parent);
        return;
    }

    screen.tile = widgets::make_bare_button(parent);
    lv_obj_add_style(screen.tile, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_size(screen.tile, LV_PCT(100), kTileHeight);
    lv_obj_set_style_radius(screen.tile, theme::kR4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.tile, theme::kS5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(screen.tile, theme::kS4, LV_PART_MAIN);
    theme::make_column(screen.tile, 0);
    lv_obj_set_flex_align(screen.tile, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    // Nagłówek: kropka stanu, nazwa i pokój, strzałka przejścia.
    lv_obj_t* head = widgets::make_box(screen.tile);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_row(head, theme::kS3);

    screen.tile_dot = widgets::make_box(head);
    lv_obj_set_size(screen.tile_dot, 10, 10);
    lv_obj_set_style_radius(screen.tile_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.tile_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.tile_dot, theme::faint(), LV_PART_MAIN);

    lv_obj_t* names = widgets::make_box(head);
    lv_obj_set_height(names, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(names, 1);
    theme::make_column(names, 3);
    lv_obj_t* title = widgets::make_text(names, theme::font_m15(), theme::text());
    lv_label_set_text(title, "Klimatyzacja");
    widgets::make_cap(names, context.room.c_str());

    lv_obj_t* go = widgets::make_text(head, theme::font_icon_sm(), theme::faint());
    lv_label_set_text(go, LV_SYMBOL_RIGHT);

    // Treść: temperatura docelowa po lewej, tryb i temperatura w pokoju po prawej.
    lv_obj_t* body = widgets::make_box(screen.tile);
    lv_obj_set_size(body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);

    lv_obj_t* temp = widgets::make_box(body);
    screen.tile_temp_box = temp;
    lv_obj_set_size(temp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temp, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(temp, 2, LV_PART_MAIN);
    screen.tile_temp = widgets::make_text(temp, theme::font_b46num(), theme::dim());
    screen.tile_unit = widgets::make_text(temp, theme::font_b19(), theme::faint());
    lv_label_set_text(screen.tile_unit, "°C");

    // Zamiast temperatury, w trybie wentylatora: podpis i bieg nawiewu.
    screen.tile_fan_box = widgets::make_box(body);
    lv_obj_set_size(screen.tile_fan_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(screen.tile_fan_box, 4);
    widgets::make_cap(screen.tile_fan_box, "MOC NAWIEWU");
    screen.tile_fan = widgets::make_text(screen.tile_fan_box, theme::font_b26(), theme::text());
    lv_label_set_long_mode(screen.tile_fan, LV_LABEL_LONG_DOT);
    // Stała szerokość: z szerokością „wg treści” i wielokropkiem LVGL pokazywał samo „…”.
    lv_obj_set_width(screen.tile_fan, 230);
    lv_obj_add_flag(screen.tile_fan_box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* meta = widgets::make_box(body);
    lv_obj_set_size(meta, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(meta, 4);
    lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    screen.tile_mode = widgets::make_text(meta, theme::font_m15(), theme::dim());
    screen.tile_now = widgets::make_text(meta, theme::font_r13(), theme::faint());

    lv_obj_add_event_cb(screen.tile, on_tile, LV_EVENT_CLICKED, &context);
}

}  // namespace ui
