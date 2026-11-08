#include <cstdio>
#include <cstring>

#include "images.h"
#include "home_internal.hpp"

// Ekran Dom: pomieszczenia (ADR 0014). Układ według makiet docs/ux/mockup-pulpit (pion)
// i docs/ux/mockup-landscape (poziom); wspólna logika makiet: docs/ux/mockup-dom/rooms.js.
//
// Dom — siatka sześciu pomieszczeń jak ekran startowy; lampka w rogu kafelka przełącza
// główne światło pokoju. Dotknięcie kafelka otwiera pokój: jego urządzenia (światło,
// w salonie także klimatyzacja), dalej światło z jasnością, barwą, efektem całej grupy
// i listą żarówek, a z niej pojedynczą żarówkę.
//
// Pilot nie zna encji. Światła przychodzą w sekcji `lights` snapshotu z krótkimi
// identyfikatorami i nimi idą polecenia `light.*` — encje zna wyłącznie bridge.

namespace ui::home {

using widgets::make_box;
using widgets::make_chevron;
using widgets::make_text;
using widgets::set_visible;

namespace {

void on_tile_lamp(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        if (context.home.tiles[i].lamp == target) {
            if (const domain::RoomLight* room = room_light(context.controller->state(), i)) {
                send_power(context, room->main);
            }
            return;
        }
    }
}

void on_tile(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        if (context.home.tiles[i].root == target) {
            open_room(context, i);
            return;
        }
    }
}

void build_climate_entry(Context& context, lv_obj_t* parent, bool wide) {
    HomeScreen& screen = context.home;
    screen.climate = card(parent);
    if (wide) {
        lv_obj_set_size(screen.climate, LV_SIZE_CONTENT, 46);
        lv_obj_set_style_radius(screen.climate, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_pad_left(screen.climate, theme::kS4, LV_PART_MAIN);
        lv_obj_set_style_pad_right(screen.climate, theme::kS3, LV_PART_MAIN);
    } else {
        lv_obj_set_size(screen.climate, LV_PCT(100), kStripHeight);
        lv_obj_set_style_radius(screen.climate, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(screen.climate, theme::kS4, LV_PART_MAIN);
    }
    theme::make_row(screen.climate, theme::kS3);
    screen.climate_dot = widgets::make_dot(screen.climate, theme::faint());
    lv_obj_t* name = make_text(screen.climate, theme::font_m15(), theme::text());
    lv_label_set_text(name, "Klimatyzacja");
    screen.climate_text = make_text(screen.climate, theme::font_r13(), theme::dim());
    if (!wide) {
        lv_obj_set_flex_grow(screen.climate_text, 1);
    }
    make_chevron(screen.climate);
    lv_obj_add_event_cb(screen.climate, open_climate, LV_EVENT_CLICKED, &context);
}

void build_tile(Context& context, lv_obj_t* grid, std::size_t index, int32_t col, int32_t row) {
    HomeScreen::RoomTile& tile = context.home.tiles[index];
    tile.root = card(grid);
    lv_obj_set_grid_cell(tile.root, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_pad_top(tile.root, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(tile.root, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(tile.root, theme::kS4, LV_PART_MAIN);
    theme::make_column(tile.root, 0);

    lv_obj_t* icon = lv_image_create(tile.root);
    lv_image_set_src(icon, kRooms[index].large);
    lv_obj_set_style_margin_left(icon, -6, LV_PART_MAIN);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* spacer = widgets::make_spacer(tile.root);
    (void)spacer;
    lv_obj_t* name = make_text(tile.root, theme::font_b26(), theme::text());
    lv_label_set_text(name, kRooms[index].name);
    lv_obj_t* row_box = make_box(tile.root);
    lv_obj_set_size(row_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_margin_top(row_box, theme::kS1, LV_PART_MAIN);
    theme::make_row(row_box, 7);
    tile.dot = widgets::make_dot(row_box, theme::faint());
    tile.state = make_text(row_box, theme::font_r13(), theme::dim());
    lv_obj_set_width(tile.state, 0);
    lv_obj_set_flex_grow(tile.state, 1);
    lv_label_set_long_mode(tile.state, LV_LABEL_LONG_DOT);

    // Lampka: osobny przycisk w rogu, poza układem kafelka.
    tile.lamp = widgets::make_bare_button(tile.root);
    lv_obj_add_flag(tile.lamp, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(tile.lamp, kChip, kChip);
    lv_obj_align(tile.lamp, LV_ALIGN_TOP_RIGHT, 4, 2);
    lv_obj_set_ext_click_area(tile.lamp, 6);
    lv_obj_set_style_radius(tile.lamp, theme::kR4 - 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile.lamp, 1, LV_PART_MAIN);
    paint_glow(tile.lamp, 0);
    tile.lamp_icon = lv_image_create(tile.lamp);
    lv_obj_align(tile.lamp_icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(tile.lamp_icon, LV_OBJ_FLAG_CLICKABLE);
    tile.lamp_bar = make_box(tile.lamp);
    lv_obj_set_size(tile.lamp_bar, kChip - 22, 3);
    lv_obj_align(tile.lamp_bar, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_radius(tile.lamp_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile.lamp_bar, 90, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tile.lamp_bar, lv_color_black(), LV_PART_MAIN);
    tile.lamp_fill = make_box(tile.lamp_bar);
    lv_obj_set_height(tile.lamp_fill, LV_PCT(100));
    lv_obj_set_style_radius(tile.lamp_fill, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile.lamp_fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tile.lamp_fill, theme::accent(), LV_PART_MAIN);
    lv_obj_add_event_cb(tile.lamp, on_tile_lamp, LV_EVENT_CLICKED, &context);

    lv_obj_add_event_cb(tile.root, on_tile, LV_EVENT_CLICKED, &context);
}

void light_summary(char* out, std::size_t size, const domain::RoomLight& room,
                   const domain::LightState& light) {
    if (!light.available) {
        std::snprintf(out, size, "%s · niedostępne", light.label.c_str());
    } else if (!light.on) {
        std::snprintf(out, size, "%s · wył.", light.label.c_str());
    } else if (&light == &room.main && !room.effect.empty()) {
        std::snprintf(out, size, "%s · %s", light.label.c_str(), effect_label(room.effect.c_str()));
    } else if (room.dimmable) {
        std::snprintf(out, size, "%s · %u%%", light.label.c_str(),
                      static_cast<unsigned>(light.brightness));
    } else {
        std::snprintf(out, size, "%s · wł.", light.label.c_str());
    }
}

void render_tiles(Context& context) {
    HomeScreen& screen = context.home;
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.lights_fresh();

    if (screen.climate != nullptr) {
        char text[48];
        climate_summary(text, sizeof(text), state);
        lv_label_set_text(screen.climate_text, text);
        const bool on = state.climate_fresh() && state.snapshot().climate.is_on();
        lv_obj_set_style_bg_color(screen.climate_dot,
                                  !state.climate_fresh() ? theme::danger()
                                  : on                   ? theme::accent()
                                                         : theme::faint(),
                                  LV_PART_MAIN);
    }

    for (std::size_t i = 0; i < kRoomCount; ++i) {
        HomeScreen::RoomTile& tile = screen.tiles[i];
        if (tile.root == nullptr) {
            continue;
        }
        const domain::RoomLight* room = room_light(state, i);
        char text[64];
        lv_color_t dot_color = theme::surface3();
        int glow = 0;
        if (room == nullptr) {
            // Bez sekcji `lights` nie wiemy, czy pokój ma światło — nie udajemy, że nie ma.
            std::snprintf(text, sizeof(text), "%s", fresh ? "brak światła" : "czekam na stan");
            lv_obj_set_style_opa(tile.lamp, 90, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(tile.lamp, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(tile.lamp, theme::line(), LV_PART_MAIN);
            lv_obj_add_flag(tile.lamp_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(tile.lamp_bar, LV_OBJ_FLAG_HIDDEN);
            widgets::set_status_outline(tile.lamp, domain::ControlStatus::Idle);
        } else {
            const domain::LightState& light = room->main;
            glow = fresh && light.available ? glow_of(*room, light) : 0;
            if (fresh) {
                light_summary(text, sizeof(text), *room, light);
                dot_color = !light.available ? theme::danger() : light.on ? theme::accent()
                                                                          : theme::faint();
            } else {
                std::snprintf(text, sizeof(text), "brak świeżego stanu");
                dot_color = theme::danger();
            }
            lv_obj_set_style_opa(tile.lamp, fresh ? LV_OPA_COVER : 128, LV_PART_MAIN);
            paint_glow(tile.lamp, glow);
            lv_obj_remove_flag(tile.lamp_icon, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(tile.lamp_icon, lamp_icon(fresh, light, glow));
            if (glow > 0 && room->dimmable) {
                lv_obj_remove_flag(tile.lamp_bar, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_width(tile.lamp_fill, LV_PCT(light.brightness));
            } else {
                lv_obj_add_flag(tile.lamp_bar, LV_OBJ_FLAG_HIDDEN);
            }
            widgets::set_status_outline(tile.lamp,
                                        status_for(state, domain::ControlId::LightPower,
                                                   light.id.c_str()));
        }
        lv_label_set_text(tile.state, text);
        lv_obj_set_style_bg_color(tile.dot, dot_color, LV_PART_MAIN);
        // Zapalone światło: ciepła obwódka całego kafelka.
        lv_obj_set_style_border_color(tile.root, glow > 0 ? theme::accent() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(tile.root, glow > 0 ? 110 : LV_OPA_COVER, LV_PART_MAIN);
    }
}

}  // namespace

}  // namespace ui::home

namespace ui {

using namespace home;

void build_home(Context& context, lv_obj_t* parent) {
    HomeScreen& screen = context.home;
    screen.section = widgets::make_section(parent);
    const bool wide = theme::landscape();

    if (wide) {
        // Poziom: klimatyzacja jako pigułka w nagłówku powłoki, pomieszczenia 3 × 2.
        lv_obj_t* slot = make_header_slot(context, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(slot, 1);
        lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        build_climate_entry(context, slot, true);
    } else {
        build_climate_entry(context, screen.section, false);
    }

    static const int32_t kCols2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t kCols3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
    static const int32_t kRows2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t kRows3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
    lv_obj_t* grid = make_box(screen.section);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, wide ? kCols3 : kCols2, wide ? kRows2 : kRows3);
    lv_obj_set_style_pad_column(grid, theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, theme::kS3, LV_PART_MAIN);
    const int32_t columns = wide ? 3 : 2;
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        build_tile(context, grid, i, static_cast<int32_t>(i) % columns,
                   static_cast<int32_t>(i) / columns);
    }
}

void render_home(Context& context) {
    render_climate(context);
    HomeScreen& screen = context.home;
    if (screen.climate != nullptr && theme::landscape()) {
        show_header_slot(context, lv_obj_get_parent(screen.climate), ScreenId::Home, true);
    }
    render_tiles(context);
    render_room(context);
}

}  // namespace ui
