#include <cstdio>
#include <cstring>

#include "images.h"
#include "home_internal.hpp"

// Zakładka Dom: światło — grupa (jasność, barwa, efekt, żarówki), pojedyncza żarówka
// i lista efektów.

namespace ui::home {

using widgets::make_box;
using widgets::make_chevron;
using widgets::make_text;
using widgets::set_visible;

namespace {

void on_kelvin(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, context.room_page.room);
    if (!state.lights_fresh() || room == nullptr || !room->has_kelvin()) {
        return;
    }
    const domain::LightState* light = current_light(context, *room);
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kKelvinSteps; ++i) {
        if (context.room_page.kelvin[i] == target && light != nullptr) {
            domain::UiIntent intent = make_intent(domain::IntentKind::SetValue, domain::ControlId::LightKelvin);
            intent.number = static_cast<int16_t>(context.room_page.kelvin_steps[i]);
            intent.target.assign(light->id.c_str());
            intent.text.assign(light->id.c_str());
            dispatch(context, intent);
            return;
        }
    }
}

void on_effect(void* raw) {
    auto& tile = *static_cast<RoomScreen::EffectTile*>(raw);
    Context& context = *tile.context;
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, context.room_page.room);
    if (!state.lights_fresh() || room == nullptr) {
        return;
    }
    // Lista znika od razu; oczekiwanie widać na kaflu efektu.
    context.room_page.effect_picker_open = false;
    widgets::show_page(context.room_page.effect_picker, false);
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::LightEffect);
    intent.target.assign(room->main.id.c_str());
    intent.text.assign(tile.name.c_str());
    intent.payload.assign(tile.name.c_str());
    dispatch(context, intent);
}

void on_bulb_row(lv_event_t* event) {
    auto& row = *static_cast<RoomScreen::BulbRow*>(lv_event_get_user_data(event));
    row.context->room_page.bulb = row.index;
    push(*row.context, Level::Bulb);
}

void on_dial(void* raw, int32_t value, bool) {
    auto& context = *static_cast<Context*>(raw);
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, context.room_page.room);
    if (!state.lights_fresh() || room == nullptr) {
        return;
    }
    const domain::LightState* light = current_light(context, *room);
    if (light == nullptr) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetValue, domain::ControlId::LightBrightness);
    intent.value = static_cast<uint8_t>(value < 1 ? 1 : value > 100 ? 100 : value);
    intent.target.assign(light->id.c_str());
    intent.text.assign(light->id.c_str());
    dispatch(context, intent);
}

lv_obj_t* tile_base(lv_obj_t* parent) {
    lv_obj_t* obj = widgets::make_bare_button(parent);
    lv_obj_set_style_radius(obj, theme::kR2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, theme::accent(), LV_PART_MAIN);
    return obj;
}

// Kafel jak „Tryb” na podstronie klimy: wybrany — miękkie bursztynowe tło i obwódka.
void paint_tile(lv_obj_t* obj, bool on) {
    lv_obj_set_style_bg_color(obj, on ? theme::accent() : theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, on ? theme::kAccentSoftOpa : theme::kOpaCover,
                            LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, on ? 1 : 0, LV_PART_MAIN);
}

const char* kelvin_name(std::size_t step) {
    static const char* const kNames[kKelvinSteps] = {"bardzo ciepła", "ciepła", "ciepła biel",
                                                     "neutralna", "biała", "chłodna", "zimna"};
    return step < kKelvinSteps ? kNames[step] : "";
}

}  // namespace

void on_light_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    if (const domain::RoomLight* room = room_light(context.controller->state(), context.room_page.room)) {
        if (const domain::LightState* light = current_light(context, *room)) {
            send_power(context, *light);
        }
    }
}

void build_light_view(Context& context, bool wide) {
    RoomScreen& screen = context.room_page;
    screen.view_light = make_box(screen.body);
    lv_obj_set_width(screen.view_light, LV_PCT(100));
    lv_obj_set_height(screen.view_light, wide ? LV_PCT(100) : LV_SIZE_CONTENT);
    lv_obj_t* side = screen.view_light;
    if (wide) {
        theme::make_row(screen.view_light, theme::kS5);
        lv_obj_set_flex_align(screen.view_light, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    } else {
        theme::make_column(screen.view_light, theme::kS2);
    }

    // Tarcza jasności: ta sama co temperatura klimy, w procentach. Obrót palcem albo −/+,
    // jedno polecenie na gest (po puszczeniu palca, 0,7 s po ostatnim stuknięciu).
    screen.dial = widgets::make_thermostat(screen.view_light, &on_dial, &context);
    screen.dial->set_percent();
    screen.dial->set_range(5, 100, 5);
    if (wide) {
        lv_obj_set_width(screen.dial->root, 320);
        side = make_box(screen.view_light);
        lv_obj_set_height(side, LV_SIZE_CONTENT);
        lv_obj_set_width(side, 0);
        lv_obj_set_flex_grow(side, 1);
    } else {
        lv_obj_set_width(screen.dial->root, LV_PCT(100));
        side = make_box(screen.view_light);
        lv_obj_set_size(side, LV_PCT(100), LV_SIZE_CONTENT);
    }
    theme::make_column(side, theme::kS2);

    // Barwa: siedem próbek od ciepłej do zimnej.
    screen.kelvin_box = make_box(side);
    lv_obj_set_size(screen.kelvin_box, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_column(screen.kelvin_box, theme::kS2);
    widgets::make_cap(screen.kelvin_box, "BARWA");
    screen.kelvin_row = make_box(screen.kelvin_box);
    lv_obj_set_size(screen.kelvin_row, LV_PCT(100), 52);
    theme::make_row(screen.kelvin_row, 6);
    for (std::size_t i = 0; i < kKelvinSteps; ++i) {
        lv_obj_t* item = widgets::make_bare_button(screen.kelvin_row);
        lv_obj_set_height(item, LV_PCT(100));
        lv_obj_set_width(item, 0);
        lv_obj_set_flex_grow(item, 1);
        lv_obj_set_style_radius(item, theme::kR2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, lv_color_hex(kKelvinColors[i]), LV_PART_MAIN);
        lv_obj_set_style_border_color(item, theme::accent(), LV_PART_MAIN);
        lv_obj_add_event_cb(item, on_kelvin, LV_EVENT_CLICKED, &context);
        screen.kelvin[i] = item;
    }
    lv_obj_t* scale = make_box(screen.kelvin_box);
    lv_obj_set_size(scale, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(scale, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scale, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_top(scale, -4, LV_PART_MAIN);
    lv_label_set_text(make_text(scale, theme::font_r11(), theme::faint()), "ciepła");
    screen.kelvin_value = make_text(scale, theme::font_r11(), theme::dim());
    lv_label_set_text(make_text(scale, theme::font_r11(), theme::faint()), "zimna");

    // Grupa: efekt całej lampy i kafle żarówek.
    screen.group_box = make_box(side);
    lv_obj_set_size(screen.group_box, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_column(screen.group_box, theme::kS2);
    lv_obj_t* effect_cap = widgets::make_cap(screen.group_box, "EFEKT · CAŁA LAMPA");
    lv_obj_set_style_margin_top(effect_cap, theme::kS1, LV_PART_MAIN);
    screen.effect_row = tile_base(screen.group_box);
    lv_obj_set_size(screen.effect_row, LV_PCT(100), 64);
    lv_obj_set_style_pad_hor(screen.effect_row, theme::kS4, LV_PART_MAIN);
    theme::make_row(screen.effect_row, theme::kS4);
    screen.effect_icon = make_text(screen.effect_row, theme::font_effect_icon(), theme::dim());
    lv_obj_t* effect_texts = make_box(screen.effect_row);
    lv_obj_set_height(effect_texts, LV_SIZE_CONTENT);
    lv_obj_set_width(effect_texts, 0);
    lv_obj_set_flex_grow(effect_texts, 1);
    theme::make_column(effect_texts, 2);
    widgets::make_cap(effect_texts, "EFEKT");
    screen.effect_value = make_text(effect_texts, theme::font_m15(), theme::text());
    make_chevron(screen.effect_row);
    lv_obj_add_event_cb(
        screen.effect_row,
        [](lv_event_t* event) { open_effect_picker(*static_cast<Context*>(lv_event_get_user_data(event))); },
        LV_EVENT_CLICKED, &context);

    lv_obj_t* bulbs_cap = widgets::make_cap(screen.group_box, "ŻARÓWKI");
    lv_obj_set_style_margin_top(bulbs_cap, theme::kS1, LV_PART_MAIN);
    lv_obj_t* row = make_box(screen.group_box);
    lv_obj_set_size(row, LV_PCT(100), wide ? 92 : 88);
    theme::make_row(row, theme::kS2);
    for (std::size_t i = 0; i < domain::kMaxLightMembers; ++i) {
        RoomScreen::BulbRow& tile = screen.bulbs[i];
        tile.context = &context;
        tile.index = i;
        tile.root = tile_base(row);
        lv_obj_set_height(tile.root, LV_PCT(100));
        lv_obj_set_width(tile.root, 0);
        lv_obj_set_flex_grow(tile.root, 1);
        theme::make_column(tile.root, 3);
        lv_obj_set_flex_align(tile.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        tile.icon = lv_image_create(tile.root);
        tile.name = widgets::make_cap(tile.root, "");
        tile.note = make_text(tile.root, theme::font_m15(), theme::text());
        lv_obj_add_event_cb(tile.root, on_bulb_row, LV_EVENT_CLICKED, &tile);
    }
}

void build_effect_picker(Context& context) {
    RoomScreen& screen = context.room_page;
    screen.effect_picker = widgets::make_page(
        "Efekt",
        [](void* raw) {
            auto& context = *static_cast<Context*>(raw);
            context.room_page.effect_picker_open = false;
            widgets::show_page(context.room_page.effect_picker, false);
        },
        &context);
    widgets::dock_sheet(screen.effect_picker);
    for (std::size_t i = 0; i < kEffectTiles; ++i) {
        RoomScreen::EffectTile& tile = screen.effects[i];
        tile.context = &context;
        tile.index = i;
        tile.row = widgets::make_list_row(screen.effect_picker.body, "", "", "", "", &on_effect, &tile);
        // Wiersz wyboru: ikona efektu zamiast symbolu, bez opisu i strzałki.
        tile.icon = lv_obj_get_child(tile.row.root, 0);
        lv_obj_set_style_text_font(tile.icon, theme::font_effect_icon(), LV_PART_MAIN);
        lv_obj_add_flag(lv_obj_get_child(tile.row.root, -1), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tile.row.note, LV_OBJ_FLAG_HIDDEN);
    }
}

void render_light_view(Context& context, const domain::RoomLight& room, bool group) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.lights_fresh();
    const domain::LightState* light_ptr = current_light(context, room);
    if (light_ptr == nullptr) {
        return;
    }
    const domain::LightState& light = *light_ptr;
    const bool usable = fresh && light.available;
    const bool on = usable && light.on;
    const bool effect_on = group && on && !room.effect.empty();

    // Barwa: siedem kroków równo w zakresie encji, zaznaczony najbliższy bieżącej.
    const domain::PendingEntry* kpending = state.pending_of(domain::ControlId::LightKelvin);
    const bool kmine = kpending != nullptr && kpending->requested == light.id.c_str();
    const int32_t current_k = kmine ? kpending->requested_number : light.kelvin;
    std::size_t nearest = kKelvinSteps;
    if (room.has_kelvin()) {
        int32_t best = 1 << 20;
        const int32_t span = room.max_kelvin - room.min_kelvin;
        for (std::size_t i = 0; i < kKelvinSteps; ++i) {
            const int32_t kelvin =
                (room.min_kelvin + span * static_cast<int32_t>(i) / (kKelvinSteps - 1) + 50) / 100 * 100;
            screen.kelvin_steps[i] = static_cast<uint16_t>(kelvin);
            const int32_t distance = current_k > kelvin ? current_k - kelvin : kelvin - current_k;
            if (current_k > 0 && distance < best) {
                best = distance;
                nearest = i;
            }
        }
    }

    // Tarcza: wartość w toku, potem potwierdzona. Pod palcem należy do tarczy.
    widgets::Thermostat& dial = *screen.dial;
    dial.set_enabled(on && room.dimmable);
    const domain::PendingEntry* bpending = state.pending_of(domain::ControlId::LightBrightness);
    const bool bmine = bpending != nullptr && bpending->requested == light.id.c_str();
    if (!dial.busy()) {
        dial.set_value(bmine ? bpending->requested_value : light.brightness);
        dial.set_status(bmine ? state.status_of(domain::ControlId::LightBrightness)
                              : domain::ControlStatus::Idle);
    }
    char note[48] = "";
    if (effect_on) {
        std::snprintf(note, sizeof(note), "%s", effect_label(room.effect.c_str()));
    } else if (on && nearest < kKelvinSteps && current_k > 0) {
        std::snprintf(note, sizeof(note), "%u K · %s", static_cast<unsigned>(current_k),
                      kelvin_name(nearest));
    } else if (!fresh) {
        std::snprintf(note, sizeof(note), "brak świeżego stanu");
    } else if (!light.available) {
        std::snprintf(note, sizeof(note), "niedostępna");
    }
    dial.set_texts(!on ? "WYŁĄCZONA" : effect_on ? "EFEKT" : "JASNOŚĆ", note);
    if (!on) {
        lv_label_set_text(dial.value_label, "—");
        lv_obj_add_flag(dial.unit_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(dial.unit_label, LV_OBJ_FLAG_HIDDEN);
    }

    set_visible(screen.kelvin_box, room.has_kelvin());
    if (room.has_kelvin()) {
        const bool active = on && !effect_on;
        for (std::size_t i = 0; i < kKelvinSteps; ++i) {
            const bool selected = active && i == nearest;
            lv_obj_set_style_bg_opa(screen.kelvin[i], !active ? 50 : selected ? LV_OPA_COVER : 140,
                                    LV_PART_MAIN);
            // Niewybrana próbka ma cienką obwódkę linii: w jasnym stylu najbielsza ginęła na tle.
            lv_obj_set_style_border_width(screen.kelvin[i], selected ? 3 : 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(screen.kelvin[i], selected ? theme::accent() : theme::line(),
                                          LV_PART_MAIN);
        }
        widgets::set_status_outline(screen.kelvin_row,
                                    kmine ? state.status_of(domain::ControlId::LightKelvin)
                                          : domain::ControlStatus::Idle);
        lv_label_set_text(screen.kelvin_value, effect_on ? "barwa nie działa w efekcie" : "");
    }

    widgets::set_power_state(screen.light_power, on, usable);
    widgets::set_status_outline(screen.light_power,
                                status_for(state, domain::ControlId::LightPower, light.id.c_str()));

    // Efekt i żarówki: tylko grupa.
    set_visible(screen.group_box, group && (room.member_count > 0 || state.snapshot().lights.effect_count > 0));
    if (!group) {
        return;
    }
    const domain::PendingEntry* effect_pending = state.pending_of(domain::ControlId::LightEffect);
    const char* effect = effect_pending != nullptr ? effect_pending->requested.c_str() : room.effect.c_str();
    lv_label_set_text(screen.effect_value, effect[0] != '\0' ? effect_label(effect) : "Brak");
    lv_label_set_text(screen.effect_icon, effect_icon(effect));
    paint_tile(screen.effect_row, effect_on);
    lv_obj_set_style_text_color(screen.effect_icon, effect_on ? theme::accent() : theme::dim(), LV_PART_MAIN);
    lv_obj_set_style_text_color(screen.effect_value, effect_on ? theme::accent() : theme::text(), LV_PART_MAIN);
    widgets::set_status_outline(screen.effect_row, state.status_of(domain::ControlId::LightEffect));
    set_visible(screen.effect_row, state.snapshot().lights.effect_count > 0);

    for (std::size_t i = 0; i < domain::kMaxLightMembers; ++i) {
        RoomScreen::BulbRow& tile = screen.bulbs[i];
        if (i >= room.member_count) {
            set_visible(tile.root, false);
            continue;
        }
        set_visible(tile.root, true);
        const domain::LightState& bulb = room.members[i];
        const bool bulb_on = fresh && bulb.available && bulb.on;
        lv_image_set_src(tile.icon, bulb_on ? &icon_bulb_on : &icon_bulb_off);
        char caps[32];
        widgets::upper_pl(caps, sizeof(caps), bulb.label.c_str());
        lv_label_set_text(tile.name, caps);
        if (!fresh || !bulb.available) {
            lv_label_set_text(tile.note, "—");
        } else if (!bulb.on) {
            lv_label_set_text(tile.note, "wył.");
        } else {
            lv_label_set_text_fmt(tile.note, "%u%%", static_cast<unsigned>(bulb.brightness));
        }
        paint_tile(tile.root, bulb_on);
        lv_obj_set_style_text_color(tile.name, bulb_on ? theme::accent() : theme::faint(), LV_PART_MAIN);
        lv_obj_set_style_text_color(tile.note, bulb_on ? theme::accent() : theme::text(), LV_PART_MAIN);
        widgets::set_status_outline(tile.root,
                                    status_for(state, domain::ControlId::LightPower, bulb.id.c_str()));
    }
}

void render_effect_picker(Context& context, const domain::RoomLight& room) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const domain::LightsState& lights = state.snapshot().lights;

    // Kolejność: znane efekty w kolejności z tabeli, potem nieznane z listy.
    std::size_t slot = 0;
    auto place = [&](const char* name) {
        if (slot >= kEffectTiles) {
            return;
        }
        RoomScreen::EffectTile& tile = screen.effects[slot++];
        tile.name.assign(name);
        lv_label_set_text(tile.row.title, name[0] != '\0' ? effect_label(name) : "Brak");
        lv_label_set_text(tile.icon, effect_icon(name));
        const bool current = state.lights_fresh() && room.effect == name;
        lv_label_set_text(tile.row.value, current ? "aktywny" : "");
        lv_obj_set_style_border_color(tile.row.root, current ? theme::accent() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_text_color(tile.icon, current ? theme::accent() : theme::dim(), LV_PART_MAIN);
        set_visible(tile.row.root, true);
    };
    place("");
    for (const EffectDef& def : kEffectNames) {
        for (std::size_t i = 0; i < lights.effect_count; ++i) {
            if (lights.effects[i] == def.id) {
                place(def.id);
                break;
            }
        }
    }
    for (std::size_t i = 0; i < lights.effect_count; ++i) {
        bool known = false;
        for (const EffectDef& def : kEffectNames) {
            known = known || lights.effects[i] == def.id;
        }
        if (!known) {
            place(lights.effects[i].c_str());
        }
    }
    for (std::size_t i = slot; i < kEffectTiles; ++i) {
        set_visible(screen.effects[i].row.root, false);
    }
}

void open_effect_picker(Context& context) {
    RoomScreen& screen = context.room_page;
    const domain::RoomLight* room = room_light(context.controller->state(), screen.room);
    if (room == nullptr || !context.controller->state().lights_fresh()) {
        return;
    }
    screen.effect_picker_open = true;
    render_effect_picker(context, *room);
    lv_obj_scroll_to_y(screen.effect_picker.body, 0, LV_ANIM_OFF);
    widgets::show_page(screen.effect_picker, true);
}

}  // namespace ui::home
