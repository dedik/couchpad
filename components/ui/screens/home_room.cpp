#include <cstdio>
#include <cstring>

#include "home_internal.hpp"
#include "images.h"

// Zakładka Dom: podstrona pomieszczenia — nagłówek, pasek pokoi, karty urządzeń i stos
// poziomów (pokój → światło → żarówka).

namespace ui::home {

using widgets::make_box;
using widgets::make_chevron;
using widgets::make_text;
using widgets::set_visible;

namespace {

void on_room_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    if (const domain::RoomLight* room = room_light(context.controller->state(), context.room_page.room)) {
        send_power(context, room->main);
    }
}

void on_back(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    if (context.room_page.depth > 0) {
        --context.room_page.depth;
        render(context);
        return;
    }
    close_room(context);
}

void on_rail(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        if (context.room_page.rail_items[i] == target) {
            context.room_page.room = i;
            context.room_page.depth = 0;
            lv_obj_scroll_to_y(context.room_page.body, 0, LV_ANIM_OFF);
            set_drawer(context, 0, true);
            render(context);
            return;
        }
    }
}


RoomScreen::Card make_card(lv_obj_t* parent, bool wide, Context& context) {
    RoomScreen::Card c;
    c.root = card(parent);
    if (wide) {
        // Poziom: dwie wysokie karty obok siebie, studzienka u góry, nazwa u dołu.
        lv_obj_set_height(c.root, LV_PCT(100));
        lv_obj_set_width(c.root, 0);
        lv_obj_set_flex_grow(c.root, 1);
        lv_obj_set_style_pad_all(c.root, theme::kS5, LV_PART_MAIN);
        theme::make_column(c.root, theme::kS1);
    } else {
        lv_obj_set_size(c.root, LV_PCT(100), kCardHeight);
        lv_obj_set_style_pad_hor(c.root, theme::kS4, LV_PART_MAIN);
        theme::make_row(c.root, theme::kS4);
    }
    c.well = make_box(c.root);
    lv_obj_set_size(c.well, 80, 80);
    lv_obj_set_style_radius(c.well, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_border_width(c.well, 1, LV_PART_MAIN);
    lv_obj_set_flex_flow(c.well, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c.well, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    paint_glow(c.well, 0);

    if (wide) {
        widgets::make_spacer(c.root);  // nazwa i stan przy dolnej krawędzi karty
    }
    lv_obj_t* texts = make_box(c.root);
    if (wide) {
        lv_obj_set_size(texts, LV_PCT(100), LV_SIZE_CONTENT);
    } else {
        lv_obj_set_height(texts, LV_SIZE_CONTENT);
        lv_obj_set_width(texts, 0);
        lv_obj_set_flex_grow(texts, 1);
    }
    theme::make_column(texts, 3);
    c.name = make_text(texts, wide ? theme::font_b26() : theme::font_b19(), theme::text());
    c.state = make_text(texts, theme::font_r13(), theme::dim());
    c.note = make_text(texts, theme::font_r11(), theme::faint());
    for (lv_obj_t* label : {c.name, c.state, c.note}) {
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    c.chevron = make_chevron(c.root);
    if (wide) {
        lv_obj_add_flag(c.chevron, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(c.chevron, LV_ALIGN_BOTTOM_RIGHT, 0, -4);
    }
    (void)context;
    return c;
}

void build_room_view(Context& context, bool wide) {
    RoomScreen& screen = context.room_page;
    screen.view_room = make_box(screen.body);
    lv_obj_set_width(screen.view_room, LV_PCT(100));
    lv_obj_set_height(screen.view_room, wide ? LV_PCT(100) : LV_SIZE_CONTENT);
    theme::make_column(screen.view_room, theme::kS2);
    widgets::make_cap(screen.view_room, "URZĄDZENIA");
    lv_obj_t* cards = make_box(screen.view_room);
    lv_obj_set_width(cards, LV_PCT(100));
    if (wide) {
        lv_obj_set_flex_grow(cards, 1);
        theme::make_row(cards, theme::kS3);
    } else {
        lv_obj_set_height(cards, LV_SIZE_CONTENT);
        theme::make_column(cards, theme::kS3);
    }

    screen.lamp = make_card(cards, wide, context);
    screen.lamp.icon = lv_image_create(screen.lamp.well);
    screen.lamp.power = widgets::make_power_button(screen.lamp.root, &on_room_power, &context);
    if (wide) {
        lv_obj_add_flag(screen.lamp.power, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(screen.lamp.power, LV_ALIGN_TOP_RIGHT, 0, 0);
    } else {
        lv_obj_move_to_index(screen.lamp.chevron, -1);  // zasilanie przed strzałką
    }
    lv_obj_add_event_cb(
        screen.lamp.root,
        [](lv_event_t* event) {
            auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
            const domain::RoomLight* room =
                room_light(context.controller->state(), context.room_page.room);
            // Sam przełącznik nie ma czego pokazać dalej — steruje nim zasilanie na karcie.
            if (room != nullptr && (room->dimmable || room->member_count > 0)) {
                push(context, Level::Light);
            }
        },
        LV_EVENT_CLICKED, &context);

    screen.climate = make_card(cards, wide, context);
    screen.climate.icon = lv_image_create(screen.climate.well);
    lv_image_set_src(screen.climate.icon, &icon_mode_cool_large);
    lv_label_set_text(screen.climate.name, "Klimatyzacja");
    lv_obj_add_event_cb(screen.climate.root, open_climate, LV_EVENT_CLICKED, &context);

    screen.empty = make_card(cards, wide, context);
    lv_obj_set_style_bg_opa(screen.empty.root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(screen.empty.well, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screen.empty.chevron, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(screen.empty.root, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(screen.empty.state, "do wybrania w konfiguracji bridge'a w HA");
}

void build_room_page(Context& context) {
    RoomScreen& screen = context.room_page;
    const bool wide = theme::landscape();

    // Pełny ekran na warstwie systemowej, jak każda podstrona. W poziomie paski są
    // schowane w szufladach (build_drawers), więc treść ma całą szerokość.
    screen.root = lv_obj_create(lv_layer_sys());
    theme::make_flat(screen.root);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(screen.root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen.root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(screen.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.root, theme::bg(), LV_PART_MAIN);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* column = screen.root;
    theme::make_column(column, 0);

    // W poziomie margines szerszy o pas chwytu gestu przy krawędzi.
    const int32_t gutter = wide ? theme::kGutterWide + 8 : theme::kGutter;
    lv_obj_t* bar = make_box(column);
    lv_obj_set_size(bar, LV_PCT(100), theme::kTopbar);
    lv_obj_set_style_pad_hor(bar, gutter, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, theme::kTopbarPad, LV_PART_MAIN);
    theme::make_row(bar, theme::kS3);
    lv_obj_t* back = widgets::make_bare_button(bar);
    lv_obj_set_size(back, 56, 56);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back, theme::surface(), LV_PART_MAIN);
    lv_obj_set_flex_flow(back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(back, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* arrow = make_text(back, theme::font_icon(), theme::dim());
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, &context);
    lv_obj_t* titles = make_box(bar);
    lv_obj_set_height(titles, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(titles, 1);
    theme::make_column(titles, 2);
    screen.title = make_text(titles, theme::font_b19(), theme::text());
    screen.sub = widgets::make_cap(titles, "");
    // Zasilanie w nagłówku po prawej, jak na każdym ekranie jednego urządzenia. Widoczne
    // na poziomach światła (grupa, żarówka, efekty); pokój ma kilka urządzeń.
    screen.light_power = widgets::make_power_button(bar, &on_light_power, &context);

    if (!wide) {
        lv_obj_t* rail = make_rail(column, context, false);
        // Linia pod paskiem pomieszczeń, wcięta jak treść.
        lv_obj_t* rule = make_box(column);
        lv_obj_set_size(rule, LV_PCT(100), 1);
        lv_obj_set_style_margin_hor(rule, theme::kGutter, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(rule, theme::surface2(), LV_PART_MAIN);
        lv_obj_set_width(rule, theme::kContentW);
        (void)rail;
    }

    screen.body = make_box(column);
    lv_obj_set_width(screen.body, LV_PCT(100));
    lv_obj_set_flex_grow(screen.body, 1);
    lv_obj_set_style_pad_hor(screen.body, gutter, LV_PART_MAIN);
    lv_obj_set_style_pad_top(screen.body, wide ? 0 : theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(screen.body, theme::kS4, LV_PART_MAIN);
    theme::make_column(screen.body, 0);
    lv_obj_add_flag(screen.body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(screen.body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(screen.body, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_width(screen.body, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(screen.body, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(screen.body, theme::faint(), LV_PART_SCROLLBAR);

    build_room_view(context, wide);
    build_light_view(context, wide);
    build_effect_picker(context);

    if (wide) {
        build_drawers(context);
    }
    screen.built = true;
}

void render_room_view(Context& context, const domain::RoomLight* room) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.lights_fresh();
    const bool climate_here = screen.room == kClimateRoom;

    set_visible(screen.lamp.root, room != nullptr);
    set_visible(screen.climate.root, climate_here);
    set_visible(screen.empty.root, room == nullptr);
    // Bez sekcji `lights` (stary bridge, brak stanu) nie wiemy, czy pokój ma światło.
    if (!fresh) {
        lv_label_set_text(screen.empty.name, "Oświetlenie");
        lv_label_set_text(screen.empty.state, "czekam na stan z Home Assistanta");
    } else {
        lv_label_set_text(screen.empty.name, climate_here ? "Inne encje" : "Brak encji");
        lv_label_set_text(screen.empty.state, "do wybrania w konfiguracji bridge'a w HA");
    }

    if (room != nullptr) {
        const domain::LightState& light = room->main;
        const int glow = fresh && light.available ? glow_of(*room, light) : 0;
        lv_label_set_text(screen.lamp.name, light.label.c_str());
        lv_image_set_src(screen.lamp.icon, lamp_icon(fresh, light, glow));
        paint_glow(screen.lamp.well, glow);
        char text[64];
        if (!fresh) {
            std::snprintf(text, sizeof(text), "brak świeżego stanu");
        } else if (!light.available) {
            std::snprintf(text, sizeof(text), "niedostępna");
        } else if (!light.on) {
            std::snprintf(text, sizeof(text), "wyłączona");
        } else if (!room->effect.empty()) {
            std::snprintf(text, sizeof(text), "efekt: %s", effect_label(room->effect.c_str()));
        } else if (room->dimmable && light.kelvin > 0) {
            std::snprintf(text, sizeof(text), "%u%% · %u K", static_cast<unsigned>(light.brightness),
                          static_cast<unsigned>(light.kelvin));
        } else if (room->dimmable) {
            std::snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(light.brightness));
        } else {
            std::snprintf(text, sizeof(text), "włączona");
        }
        lv_label_set_text(screen.lamp.state, text);
        if (room->member_count > 0) {
            std::size_t lit = 0;
            for (std::size_t i = 0; i < room->member_count; ++i) {
                lit += room->members[i].on ? 1 : 0;
            }
            std::snprintf(text, sizeof(text), "%u z %u żarówek", static_cast<unsigned>(lit),
                          static_cast<unsigned>(room->member_count));
        } else {
            text[0] = '\0';
        }
        lv_label_set_text(screen.lamp.note, text);
        set_visible(screen.lamp.chevron, room->dimmable || room->member_count > 0);
        widgets::set_power_state(screen.lamp.power, fresh && light.on, fresh && light.available);
        widgets::set_status_outline(screen.lamp.power,
                                    status_for(state, domain::ControlId::LightPower, light.id.c_str()));
    }

    if (climate_here) {
        char text[48];
        climate_summary(text, sizeof(text), state);
        lv_label_set_text(screen.climate.state, text);
        const domain::ClimateState& climate = state.snapshot().climate;
        if (state.climate_fresh() && climate.has_current) {
            char temp[16];
            widgets::format_tenths(temp, sizeof(temp), climate.current_tenths);
            lv_label_set_text_fmt(screen.climate.note, "w pokoju %s°", temp);
        } else {
            lv_label_set_text(screen.climate.note, "");
        }
        const bool on = state.climate_fresh() && climate.is_on();
        paint_glow(screen.climate.well, on ? 1 : 0);
    }
}

}  // namespace

// Karta i pasek klimatyzacji otwierają jej podstronę (screen_climate.cpp).
void open_climate(lv_event_t* event) {
    open_climate_page(*static_cast<Context*>(lv_event_get_user_data(event)));
}

void push(Context& context, Level level) {
    RoomScreen& screen = context.room_page;
    if (screen.depth < 3) {
        screen.stack[screen.depth++] = level;
    }
    lv_obj_scroll_to_y(screen.body, 0, LV_ANIM_OFF);
    render(context);
}

lv_obj_t* make_rail(lv_obj_t* parent, Context& context, bool wide) {
    lv_obj_t* rail = make_box(parent);
    RoomScreen& screen = context.room_page;
    if (wide) {
        lv_obj_set_size(rail, theme::kRail, LV_PCT(100));
        lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(rail, theme::surface(), LV_PART_MAIN);
        lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_width(rail, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(rail, theme::line(), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(rail, theme::kS2, LV_PART_MAIN);
        theme::make_column(rail, theme::kS2);
        lv_obj_set_flex_align(rail, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_size(rail, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(rail, theme::kGutter, LV_PART_MAIN);
        lv_obj_set_style_pad_top(rail, theme::kS3, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(rail, 6, LV_PART_MAIN);
        theme::make_row(rail, 6);
    }
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        lv_obj_t* item = widgets::make_bare_button(rail);
        lv_obj_set_height(item, kRailItem);
        if (wide) {
            lv_obj_set_width(item, LV_PCT(100));
        } else {
            lv_obj_set_width(item, 0);
            lv_obj_set_flex_grow(item, 1);
        }
        lv_obj_set_style_radius(item, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, theme::accent(), LV_PART_MAIN);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* icon = lv_image_create(item);
        lv_image_set_src(icon, kRooms[i].icon);
        // Znacznik wybranego pokoju: kreska przy krawędzi, jak przy zakładce.
        lv_obj_set_style_border_color(item, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_border_side(item, wide ? LV_BORDER_SIDE_RIGHT : LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN);
        lv_obj_add_event_cb(item, on_rail, LV_EVENT_CLICKED, &context);
        screen.rail_items[i] = item;
    }
    return rail;
}

void open_room(Context& context, std::size_t index) {
    RoomScreen& screen = context.room_page;
    if (!screen.built) {
        build_room_page(context);
    }
    screen.room = index;
    screen.depth = 0;
    screen.open = true;
    set_drawer(context, 0, false);
    lv_obj_remove_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(screen.root);
    lv_obj_scroll_to_y(screen.body, 0, LV_ANIM_OFF);
    render(context);
}

void render_room(Context& context) {
    RoomScreen& screen = context.room_page;
    if (!screen.built || !screen.open) {
        return;
    }
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, screen.room);
    // Poziom głębszy niż pokój bez światła (np. bridge przestał je podawać) — wracamy.
    if (room == nullptr) {
        screen.depth = 0;
    }
    const Level level = screen.depth > 0 ? screen.stack[screen.depth - 1] : Level::Room;
    if (level == Level::Bulb && (room == nullptr || screen.bulb >= room->member_count)) {
        screen.depth = 0;
    }
    const Level shown = screen.depth > 0 ? screen.stack[screen.depth - 1] : Level::Room;

    for (std::size_t i = 0; i < kRoomCount; ++i) {
        const bool selected = i == screen.room;
        lv_obj_t* item = screen.rail_items[i];
        lv_obj_set_style_bg_opa(item, selected ? theme::kAccentSoftOpa : theme::kOpaTransp, LV_PART_MAIN);
        lv_obj_set_style_border_width(item, selected ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_image_opa(lv_obj_get_child(item, 0), selected ? LV_OPA_COVER : 150, LV_PART_MAIN);
    }

    set_visible(screen.light_power, shown != Level::Room);
    set_visible(screen.view_room, shown == Level::Room);
    set_visible(screen.view_light, shown == Level::Light || shown == Level::Bulb);
    if (screen.effect_picker_open && room != nullptr) {
        render_effect_picker(context, *room);
    }

    char sub[64];
    if (shown == Level::Room) {
        lv_label_set_text(screen.title, kRooms[screen.room].name);
        lv_label_set_text(screen.sub, "POMIESZCZENIE");
        render_room_view(context, room);
        return;
    }
    const bool group = shown == Level::Light;
    if (group) {
        lv_label_set_text(screen.title, room->main.label.c_str());
        if (room->member_count > 0) {
            std::snprintf(sub, sizeof(sub), "%s · %u ŻARÓWKI", kRooms[screen.room].caps,
                          static_cast<unsigned>(room->member_count));
        } else {
            std::snprintf(sub, sizeof(sub), "%s", kRooms[screen.room].caps);
        }
    } else {
        std::snprintf(sub, sizeof(sub), "Żarówka %s", room->members[screen.bulb].label.c_str());
        lv_label_set_text(screen.title, sub);
        char caps[32];
        widgets::upper_pl(caps, sizeof(caps), room->main.label.c_str());
        std::snprintf(sub, sizeof(sub), "%s · %s", caps, kRooms[screen.room].caps);
    }
    lv_label_set_text(screen.sub, sub);
    render_light_view(context, *room, group);
}

}  // namespace ui::home

namespace ui {

using namespace home;

void close_room(Context& context) {
    RoomScreen& screen = context.room_page;
    if (!screen.built) {
        return;
    }
    screen.open = false;
    screen.depth = 0;
    screen.effect_picker_open = false;
    widgets::show_page(screen.effect_picker, false);
    set_drawer(context, 0, false);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace ui
