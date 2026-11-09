#include <algorithm>
#include <cstdio>
#include <cstring>

#include "home_internal.hpp"
#include "images.h"

// Zakładka Dom: grzejnik (ADR 0018) — karta w pokoju i ekran głowicy termostatycznej.
//
// Temperatura w pokoju zawsze z czujnika pomieszczenia (wiadomość pogody, ADR 0017), nigdy
// z głowicy: ta mierzy przy kaloryferze i zawyża odczyt. Snapshot jej zresztą nie podaje.

namespace ui::home {

using widgets::make_box;
using widgets::make_text;
using widgets::set_visible;

namespace {

// Skala tarczy: encja pozwala 4–35 °C, ale skrajne wartości nie mają sensu na co dzień.
constexpr int16_t kDialMin = 50;
constexpr int16_t kDialMax = 300;
constexpr int16_t kQuickTenths[RoomScreen::kQuickTargets] = {170, 190, 210, 230};
constexpr uint8_t kBatteryLow = 30;

const char* mode_name(const domain::RadiatorState& r) {
    if (!r.is_on()) {
        return "Wyłączony";
    }
    if (r.mode == "auto") {
        return "Harmonogram";
    }
    if (r.mode == "heat") {
        return "Grzanie";
    }
    return r.mode.c_str();  // nieznany tryb pokazujemy tak, jak przyszedł
}

const char* mode_caps(const domain::RadiatorState& r) {
    if (!r.is_on()) {
        return "WYŁĄCZONY";
    }
    return r.mode == "auto" ? "HARMONOGRAM" : r.mode == "heat" ? "GRZANIE" : "";
}

// Nastawa, którą pokazujemy: w toku (dotknięcie czeka na potwierdzenie) albo potwierdzona.
int16_t shown_target(const domain::AppState& state, const domain::RadiatorState& r) {
    const domain::PendingEntry* pending = state.pending_of(domain::ControlId::RadiatorTarget);
    return pending != nullptr && pending->requested == r.id.c_str() ? pending->requested_number
                                                                    : r.target_tenths;
}

void send_target(Context& context, const domain::RadiatorState& r, int32_t tenths) {
    domain::UiIntent intent = make_intent(domain::IntentKind::SetValue, domain::ControlId::RadiatorTarget);
    intent.number = static_cast<int16_t>(std::clamp<int32_t>(tenths, r.min_tenths, r.max_tenths));
    intent.target.assign(r.id.c_str());
    intent.text.assign(r.id.c_str());
    dispatch(context, intent);
}

const domain::RadiatorState* current(Context& context, bool usable_only) {
    const domain::AppState& state = context.controller->state();
    const domain::RadiatorState* r = room_radiator(state, context.room_page.room);
    if (r == nullptr || (usable_only && (!state.radiators_fresh() || !r->available))) {
        return nullptr;
    }
    return r;
}

void on_dial(void* raw, int32_t value, bool) {
    auto& context = *static_cast<Context*>(raw);
    if (const domain::RadiatorState* r = current(context, true); r != nullptr && r->is_on()) {
        send_target(context, *r, value);
    }
}

void on_mode(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    const domain::RadiatorState* r = current(context, true);
    if (r == nullptr) {
        return;
    }
    const bool heat = lv_event_get_current_target(event) == context.room_page.rad_heat;
    const char* mode = heat ? "heat" : "auto";
    if (r->is_on() && r->mode == mode) {
        return;
    }
    // Wybór trybu przy wyłączonej głowicy zarazem ją włącza (climate.set_hvac_mode).
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::RadiatorMode);
    intent.target.assign(r->id.c_str());
    intent.text.assign(mode);
    dispatch(context, intent);
}

void on_quick(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    const domain::RadiatorState* r = current(context, true);
    if (r == nullptr) {
        return;
    }
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < RoomScreen::kQuickTargets; ++i) {
        if (context.room_page.rad_quick[i] == target) {
            send_target(context, *r, kQuickTenths[i]);
            return;
        }
    }
}

RoomScreen::StatTile make_stat(lv_obj_t* parent) {
    RoomScreen::StatTile t;
    t.root = make_box(parent);
    lv_obj_set_height(t.root, 84);
    lv_obj_set_width(t.root, 0);
    lv_obj_set_flex_grow(t.root, 1);
    lv_obj_set_style_radius(t.root, theme::kR2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(t.root, theme::surface2(), LV_PART_MAIN);
    theme::make_column(t.root, 3);
    lv_obj_set_flex_align(t.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    t.icon = lv_image_create(t.root);
    t.cap = widgets::make_cap(t.root, "");
    t.value = make_text(t.root, theme::font_m15(), theme::text());
    lv_obj_set_width(t.value, LV_PCT(100));
    lv_obj_set_style_text_align(t.value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(t.value, LV_LABEL_LONG_DOT);
    return t;
}

void paint_stat(RoomScreen::StatTile& t, const lv_image_dsc_t* icon, const char* cap, const char* value,
                lv_color_t color) {
    lv_image_set_src(t.icon, icon);
    lv_label_set_text(t.cap, cap);
    lv_label_set_text(t.value, value);
    lv_obj_set_style_text_color(t.value, color, LV_PART_MAIN);
}

lv_color_t heat_color() { return lv_color_hex(0xff7e5f); }

lv_obj_t* make_row(lv_obj_t* parent, int32_t height) {
    lv_obj_t* row = make_box(parent);
    lv_obj_set_size(row, LV_PCT(100), height);
    theme::make_row(row, theme::kS2);
    return row;
}

}  // namespace

const domain::RadiatorState* room_radiator(const domain::AppState& state, std::size_t room) {
    if (room >= kRoomCount) {
        return nullptr;
    }
    return state.snapshot().radiators.room(kRooms[room].id);
}

void on_radiator_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    if (const domain::RadiatorState* r = current(context, true)) {
        domain::UiIntent intent = make_intent(domain::IntentKind::SetPower, domain::ControlId::RadiatorPower);
        intent.flag = !r->is_on();
        intent.target.assign(r->id.c_str());
        intent.text.assign(r->id.c_str());
        dispatch(context, intent);
    }
}

void build_radiator_card(Context& context, RoomScreen::Card& card) {
    card.icon = lv_image_create(card.well);
    lv_label_set_text(card.name, "Grzejnik");
    lv_obj_add_event_cb(
        card.root,
        [](lv_event_t* event) {
            auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
            if (room_radiator(context.controller->state(), context.room_page.room) != nullptr) {
                push(context, Level::Radiator);
            }
        },
        LV_EVENT_CLICKED, &context);
}

void render_radiator_card(Context& context, RoomScreen::Card& card, const domain::RadiatorState& r) {
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.radiators_fresh() && r.available;
    const bool heats = fresh && r.heats();
    lv_image_set_src(card.icon, heats ? &icon_dev_radiator : &icon_dev_radiator_idle);
    lv_obj_set_style_image_opa(card.icon, fresh && r.is_on() ? LV_OPA_COVER : 140, LV_PART_MAIN);
    paint_glow(card.well, heats ? 1 : 0);

    char text[48];
    if (!state.radiators_fresh()) {
        std::snprintf(text, sizeof(text), "brak świeżego stanu");
    } else if (!r.available) {
        std::snprintf(text, sizeof(text), "niedostępny");
    } else if (!r.is_on()) {
        std::snprintf(text, sizeof(text), "Wyłączony");
    } else {
        char temp[16];
        widgets::format_tenths(temp, sizeof(temp), r.target_tenths);
        std::snprintf(text, sizeof(text), "%s · %s°", mode_name(r), temp);
    }
    lv_label_set_text(card.state, text);

    // Drugi wiersz: temperatura z czujnika pokoju i co robi zawór.
    const char* doing = !fresh || !r.is_on() ? "" : r.window == 1 ? "okno otwarte" : heats ? "grzeje" : "nie grzeje";
    int16_t room_t = 0;
    if (room_temperature(context, context.room_page.room, room_t)) {
        char temp[16];
        widgets::format_tenths(temp, sizeof(temp), room_t);
        std::snprintf(text, sizeof(text), "w pokoju %s°%s%s", temp, doing[0] != '\0' ? " · " : "", doing);
    } else {
        std::snprintf(text, sizeof(text), "%s", doing);
    }
    lv_label_set_text(card.note, text);
    widgets::set_status_outline(card.root, state.pending_of(domain::ControlId::RadiatorPower) != nullptr
                                               ? state.status_of(domain::ControlId::RadiatorPower)
                                               : domain::ControlStatus::Idle);
}

void build_radiator_view(Context& context, bool wide) {
    RoomScreen& screen = context.room_page;
    screen.view_rad = make_box(screen.body);
    lv_obj_set_width(screen.view_rad, LV_PCT(100));
    lv_obj_set_height(screen.view_rad, wide ? LV_PCT(100) : LV_SIZE_CONTENT);
    if (wide) {
        theme::make_row(screen.view_rad, theme::kS5);
        lv_obj_set_flex_align(screen.view_rad, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        theme::make_column(screen.view_rad, theme::kS2);
    }

    // Tarcza nastawy jak przy klimie, łuk w barwach od zimnej (niebieski) do gorącej (czerwień).
    screen.rad_dial = widgets::make_thermostat(screen.view_rad, &on_dial, &context);
    screen.rad_dial->set_gradient(&ring_heat);
    screen.rad_dial->set_range(kDialMin, kDialMax, 5);
    lv_obj_set_width(screen.rad_dial->root, wide ? 320 : LV_PCT(100));

    lv_obj_t* side = make_box(screen.view_rad);
    if (wide) {
        lv_obj_set_height(side, LV_SIZE_CONTENT);
        lv_obj_set_width(side, 0);
        lv_obj_set_flex_grow(side, 1);
    } else {
        lv_obj_set_size(side, LV_PCT(100), LV_SIZE_CONTENT);
    }
    theme::make_column(side, theme::kS2);

    widgets::make_cap(side, "TRYB");
    screen.rad_mode_row = make_row(side, 64);
    for (lv_obj_t** slot : {&screen.rad_heat, &screen.rad_auto}) {
        lv_obj_t* tile = tile_base(screen.rad_mode_row);
        lv_obj_set_height(tile, LV_PCT(100));
        lv_obj_set_width(tile, 0);
        lv_obj_set_flex_grow(tile, 1);
        theme::make_row(tile, theme::kS2);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        const bool heat = slot == &screen.rad_heat;
        lv_obj_t* icon = lv_image_create(tile);
        lv_image_set_src(icon, heat ? &icon_mode_heat : &icon_trv_auto);
        lv_obj_t* label = make_text(tile, theme::font_m15(), theme::text());
        lv_label_set_text(label, heat ? "Grzanie" : "Harmonogram");
        lv_obj_add_event_cb(tile, on_mode, LV_EVENT_CLICKED, &context);
        *slot = tile;
    }

    widgets::make_cap(side, "TERAZ");
    lv_obj_t* now = make_row(side, 84);
    lv_obj_set_style_pad_column(now, 6, LV_PART_MAIN);
    screen.rad_valve = make_stat(now);
    screen.rad_window = make_stat(now);
    screen.rad_battery = make_stat(now);
    screen.rad_extra = make_stat(now);

    widgets::make_cap(side, "SZYBKO");
    lv_obj_t* quick = make_row(side, 56);
    for (std::size_t i = 0; i < RoomScreen::kQuickTargets; ++i) {
        lv_obj_t* tile = tile_base(quick);
        lv_obj_set_height(tile, LV_PCT(100));
        lv_obj_set_width(tile, 0);
        lv_obj_set_flex_grow(tile, 1);
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* label = make_text(tile, theme::font_b19(), theme::text());
        lv_label_set_text_fmt(label, "%d°", kQuickTenths[i] / 10);
        lv_obj_add_event_cb(tile, on_quick, LV_EVENT_CLICKED, &context);
        screen.rad_quick[i] = tile;
    }
}

void render_radiator_view(Context& context, const domain::RadiatorState& r) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const bool usable = state.radiators_fresh() && r.available;
    const bool on = usable && r.is_on();
    const int16_t target = shown_target(state, r);

    int16_t room_t = 0;
    const bool has_room = room_temperature(context, screen.room, room_t);

    // Tarcza: zakres z encji przycięty do skali, pod palcem wartość należy do tarczy.
    widgets::Thermostat& dial = *screen.rad_dial;
    dial.set_range(std::max<int16_t>(r.min_tenths, kDialMin), std::min<int16_t>(r.max_tenths, kDialMax),
                   r.step_tenths);
    dial.set_enabled(on);
    const domain::PendingEntry* pending = state.pending_of(domain::ControlId::RadiatorTarget);
    const bool mine = pending != nullptr && pending->requested == r.id.c_str();
    if (!dial.busy()) {
        dial.set_value(target);
        dial.set_status(mine ? state.status_of(domain::ControlId::RadiatorTarget) : domain::ControlStatus::Idle);
    }
    dial.set_marker(has_room, room_t);
    char note[40] = "";
    if (!state.radiators_fresh()) {
        std::snprintf(note, sizeof(note), "brak świeżego stanu");
    } else if (!r.available) {
        std::snprintf(note, sizeof(note), "niedostępny");
    } else if (has_room) {
        char temp[16];
        widgets::format_tenths(temp, sizeof(temp), room_t);
        std::snprintf(note, sizeof(note), "w pokoju %s°", temp);
    }
    dial.set_texts(usable ? mode_caps(r) : "GRZEJNIK", note);
    if (!on) {
        lv_label_set_text(dial.value_label, "—");
        lv_obj_add_flag(dial.unit_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(dial.unit_label, LV_OBJ_FLAG_HIDDEN);
    }

    // Tryb: wybrany kafel (albo ten, o który właśnie poproszono).
    const domain::PendingEntry* mpending = state.pending_of(domain::ControlId::RadiatorMode);
    const char* mode = mpending != nullptr ? mpending->requested.c_str() : on ? r.mode.c_str() : "";
    set_visible(screen.rad_heat, r.has_heat);
    set_visible(screen.rad_auto, r.has_auto);
    for (lv_obj_t* tile : {screen.rad_heat, screen.rad_auto}) {
        const bool selected = std::strcmp(mode, tile == screen.rad_heat ? "heat" : "auto") == 0;
        paint_tile(tile, selected);
        widgets::set_status_outline(tile, selected && mpending != nullptr
                                              ? state.status_of(domain::ControlId::RadiatorMode)
                                              : domain::ControlStatus::Idle);
        lv_obj_set_style_opa(tile, usable ? LV_OPA_COVER : 110, LV_PART_MAIN);
    }

    // Teraz: zawór, okno (tylko z kontaktronem), bateria, następny punkt albo cel – pokój.
    const bool heats = usable && r.heats();
    paint_stat(screen.rad_valve, heats ? &icon_trv_valve_on : &icon_trv_valve_off, "ZAWÓR",
               !on ? "wyłączony" : heats ? "grzeje" : "nie grzeje",
               heats ? heat_color() : on && r.window == 1 ? theme::accent() : theme::text());
    set_visible(screen.rad_window.root, r.window >= 0);
    if (r.window >= 0) {
        paint_stat(screen.rad_window, r.window == 1 ? &icon_trv_window_open : &icon_trv_window_closed, "OKNO",
                   r.window == 1 ? "otwarte" : "zamknięte", r.window == 1 ? theme::accent() : theme::text());
    }
    set_visible(screen.rad_battery.root, r.battery >= 0);
    if (r.battery >= 0) {
        char value[8];
        std::snprintf(value, sizeof(value), "%d%%", static_cast<int>(r.battery));
        const bool low = r.battery < kBatteryLow;
        paint_stat(screen.rad_battery, low ? &icon_trv_battery_low : &icon_trv_battery_ok, "BATERIA", value,
                   low ? theme::accent() : theme::text());
    }
    char value[24];
    char cap[16];
    if (on && r.mode == "auto" && r.has_next) {
        char temp[16];
        widgets::format_tenths(temp, sizeof(temp), r.next_tenths);
        std::snprintf(cap, sizeof(cap), "OD %s", r.next_at.c_str());
        std::snprintf(value, sizeof(value), "%s°", temp);
        paint_stat(screen.rad_extra, &icon_trv_next, cap, value, theme::text());
    } else {
        if (on && has_room) {
            const int delta = target - room_t;
            char temp[16];
            widgets::format_tenths(temp, sizeof(temp), delta < 0 ? -delta : delta);
            std::snprintf(value, sizeof(value), "%s%s°", delta > 0 ? "+" : delta < 0 ? "-" : "", temp);
        } else {
            std::snprintf(value, sizeof(value), "—");
        }
        paint_stat(screen.rad_extra, &icon_trv_delta, "CEL – POKÓJ", value, theme::text());
    }

    // Szybko: nastawa jednym dotknięciem; wybrany kafel = bieżąca nastawa.
    for (std::size_t i = 0; i < RoomScreen::kQuickTargets; ++i) {
        const bool selected = on && target == kQuickTenths[i];
        paint_tile(screen.rad_quick[i], selected);
        widgets::set_status_outline(screen.rad_quick[i], selected && mine
                                                             ? state.status_of(domain::ControlId::RadiatorTarget)
                                                             : domain::ControlStatus::Idle);
        lv_obj_set_style_opa(screen.rad_quick[i], on ? LV_OPA_COVER : 110, LV_PART_MAIN);
    }

    widgets::set_power_state(screen.light_power, on, usable);
    widgets::set_status_outline(screen.light_power, state.pending_of(domain::ControlId::RadiatorPower) != nullptr
                                                        ? state.status_of(domain::ControlId::RadiatorPower)
                                                        : domain::ControlStatus::Idle);
}

}  // namespace ui::home
