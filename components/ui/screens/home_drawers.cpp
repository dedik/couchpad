#include <cstdio>
#include <cstring>

#include "images.h"
#include "home_internal.hpp"

// Zakładka Dom w poziomie: szuflady przy krawędziach — zakładki z lewej, pokoje z prawej.
// Przeciągnięcie od krawędzi albo stuknięcie w pas wysuwa szufladę, przyciemnienie ją chowa.

namespace ui::home {

using widgets::make_box;
using widgets::make_chevron;
using widgets::make_text;
using widgets::set_visible;

namespace {

constexpr int32_t kEdgeZone = 28;   // pas chwytu gestu przy krawędzi
constexpr int32_t kSwipe = 36;      // tyle trzeba przeciągnąć w głąb ekranu
constexpr uint32_t kDrawerMs = 180;

int32_t screen_width() {
    return lv_display_get_horizontal_resolution(lv_display_get_default());
}

int32_t touch_x() {
    lv_point_t point{};
    if (lv_indev_t* indev = lv_indev_active()) {
        lv_indev_get_point(indev, &point);
    }
    return point.x;
}

void slide(lv_obj_t* obj, int32_t to, bool animate) {
    lv_anim_delete(obj, nullptr);
    if (!animate) {
        lv_obj_set_x(obj, to);
        return;
    }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, lv_obj_get_x(obj), to);
    lv_anim_set_duration(&anim, kDrawerMs);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, [](void* target, int32_t value) {
        lv_obj_set_x(static_cast<lv_obj_t*>(target), value);
    });
    lv_anim_start(&anim);
}

// Pas przy krawędzi: przeciągnięcie w głąb ekranu wysuwa szufladę, samo dotknięcie też
// (krawędź panelu bywa mniej czuła, a stuknięcie w uchwyt jest oczywiste).
void on_edge(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    RoomScreen& screen = context.room_page;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    const int8_t side = target == screen.edges[0] ? -1 : 1;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            screen.swipe = side;
            screen.swipe_x = touch_x();
            break;
        case LV_EVENT_PRESSING:
            if (screen.swipe == side && side * (screen.swipe_x - touch_x()) <= -kSwipe) {
                set_drawer(context, side, true);
            }
            break;
        case LV_EVENT_CLICKED:
            if (screen.drawer == 0) {
                set_drawer(context, side, true);
            }
            break;
        default:
            break;
    }
}

// Przyciemnienie: dotknięcie chowa szufladę, przeciągnięcie w stronę jej krawędzi też.
void on_scrim(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    RoomScreen& screen = context.room_page;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            screen.swipe_x = touch_x();
            break;
        case LV_EVENT_PRESSING:
            if (screen.drawer != 0 && screen.drawer * (touch_x() - screen.swipe_x) >= kSwipe) {
                set_drawer(context, 0, true);
            }
            break;
        case LV_EVENT_CLICKED:
            set_drawer(context, 0, true);
            break;
        default:
            break;
    }
}

void on_drawer_tab(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    const auto index = static_cast<std::size_t>(lv_obj_get_index(target));
    if (index < kScreenCount) {
        close_room(context);
        show_screen(context, static_cast<ScreenId>(index));
    }
}

}  // namespace

void set_drawer(Context& context, int8_t side, bool animate) {
    RoomScreen& screen = context.room_page;
    if (screen.drawer_tabs == nullptr) {
        return;
    }
    screen.drawer = side;
    screen.swipe = 0;
    const int32_t width = screen_width();
    slide(screen.drawer_tabs, side == -1 ? 0 : -theme::kRail - 12, animate);
    slide(screen.drawer_rooms, side == 1 ? width - theme::kRail : width + 12, animate);
    widgets::set_visible(screen.scrim, side != 0);
}

void build_drawers(Context& context) {
    RoomScreen& screen = context.room_page;
    const int32_t width = screen_width();

    for (int i = 0; i < 2; ++i) {
        lv_obj_t* edge = widgets::make_bare_button(screen.root);
        lv_obj_add_flag(edge, LV_OBJ_FLAG_FLOATING);
        lv_obj_remove_flag(edge, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_set_size(edge, kEdgeZone, LV_PCT(100));
        lv_obj_set_pos(edge, i == 0 ? 0 : width - kEdgeZone, 0);
        // Uchwyt: podpowiedź, że przy krawędzi coś jest.
        lv_obj_t* handle = make_box(edge);
        lv_obj_set_size(handle, 5, 64);
        lv_obj_set_style_radius(handle, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(handle, theme::surface3(), LV_PART_MAIN);
        lv_obj_align(handle, i == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, i == 0 ? 5 : -5, 0);
        for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_CLICKED}) {
            lv_obj_add_event_cb(edge, on_edge, code, &context);
        }
        screen.edges[i] = edge;
    }

    screen.scrim = widgets::make_bare_button(screen.root);
    lv_obj_add_flag(screen.scrim, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(screen.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(screen.scrim, 0, 0);
    lv_obj_set_style_bg_opa(screen.scrim, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.scrim, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_flag(screen.scrim, LV_OBJ_FLAG_HIDDEN);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_CLICKED}) {
        lv_obj_add_event_cb(screen.scrim, on_scrim, code, &context);
    }

    // Lewa szuflada: zakładki jak w pasku powłoki; Dom zaznaczony.
    static const lv_image_dsc_t* const kTabIcons[kScreenCount] = {
        &icon_tv, &icon_appletv, &icon_audio, &icon_home, &icon_settings};
    screen.drawer_tabs = make_box(screen.root);
    lv_obj_add_flag(screen.drawer_tabs, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.drawer_tabs, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(screen.drawer_tabs, theme::kRail, LV_PCT(100));
    lv_obj_set_style_bg_opa(screen.drawer_tabs, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.drawer_tabs, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_side(screen.drawer_tabs, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen.drawer_tabs, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(screen.drawer_tabs, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.drawer_tabs, theme::kS2, LV_PART_MAIN);
    theme::make_column(screen.drawer_tabs, theme::kS2);
    lv_obj_set_flex_align(screen.drawer_tabs, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    for (std::size_t i = 0; i < kScreenCount; ++i) {
        lv_obj_t* tab = widgets::make_bare_button(screen.drawer_tabs);
        lv_obj_set_size(tab, LV_PCT(100), theme::kRailTab);
        lv_obj_set_style_radius(tab, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tab, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tab, i == static_cast<std::size_t>(ScreenId::Home)
                                         ? theme::kAccentSoftOpa
                                         : theme::kOpaTransp,
                                LV_PART_MAIN);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* icon = lv_image_create(tab);
        lv_image_set_src(icon, kTabIcons[i]);
        lv_obj_add_event_cb(tab, on_drawer_tab, LV_EVENT_CLICKED, &context);
    }

    // Prawa szuflada: pasek pomieszczeń.
    screen.drawer_rooms = make_rail(screen.root, context, true);
    lv_obj_add_flag(screen.drawer_rooms, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.drawer_rooms, LV_OBJ_FLAG_CLICKABLE);

    for (lv_obj_t* drawer : {screen.drawer_tabs, screen.drawer_rooms}) {
        lv_obj_set_y(drawer, 0);
        lv_obj_set_style_shadow_width(drawer, 30, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(drawer, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(drawer, lv_color_black(), LV_PART_MAIN);
    }
    set_drawer(context, 0, false);
}

}  // namespace ui::home
