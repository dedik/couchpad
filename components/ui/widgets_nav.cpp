#include "widgets_internal.hpp"

#include <cmath>
#include <cstdlib>

#include "storage/settings.hpp"

namespace ui::widgets {

using namespace detail;

namespace {

// Krzyżak urósł po zdjęciu paska statusu i ikon zakładek (2026-11-03). Szerokość
// wyznaczają kołyski po bokach: 72 + 16 + 256 + 16 + 72 = 432.
constexpr int32_t kPadSize = 256;
constexpr int32_t kPadKey = 80;
constexpr int32_t kPadOk = 112;
constexpr int32_t kPadKeyMid = (kPadSize - kPadKey) / 2;
constexpr int32_t kPadOkPos = (kPadSize - kPadOk) / 2;
constexpr int32_t kTrackpadSize = 240;
constexpr int32_t kTrackpadKey = 56;
constexpr int32_t kTrackpadCenter = 84;

}  // namespace

// ============================= krzyżak =============================

namespace {

struct KeyBinding {
    KeyHandler handler = nullptr;
    void* context = nullptr;
    domain::NavKey key = domain::NavKey::None;
};

lv_obj_t* pad_key(lv_obj_t* parent, const char* symbol, domain::NavKey key, KeyHandler handler,
                  void* context, int32_t size, int32_t radius) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, size, size);
    lv_obj_set_style_radius(button, radius, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, theme::dim(), LV_PART_MAIN);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    inherited_icon(button, symbol);

    auto* binding = static_cast<KeyBinding*>(lv_malloc(sizeof(KeyBinding)));
    binding->handler = handler;
    binding->context = context;
    binding->key = key;
    lv_obj_add_event_cb(
        button,
        [](lv_event_t* event) {
            auto* data = static_cast<KeyBinding*>(lv_event_get_user_data(event));
            data->handler(data->context, data->key);
        },
        LV_EVENT_CLICKED, binding);
    lv_obj_add_event_cb(
        button, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); }, LV_EVENT_DELETE,
        binding);
    return button;
}

}  // namespace

lv_obj_t* make_dpad(lv_obj_t* parent, KeyHandler on_key, void* context) {
    lv_obj_t* pad = plain(parent);
    lv_obj_set_size(pad, kPadSize, kPadSize);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pad, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(pad, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(pad, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_radius(pad, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_t* up = pad_key(pad, LV_SYMBOL_UP, domain::NavKey::Up, on_key, context, kPadKey,
                           theme::kR4);
    lv_obj_set_pos(up, kPadKeyMid, 2);
    lv_obj_t* down = pad_key(pad, LV_SYMBOL_DOWN, domain::NavKey::Down, on_key, context, kPadKey,
                             theme::kR4);
    lv_obj_set_pos(down, kPadKeyMid, kPadSize - kPadKey - 2);
    lv_obj_t* left = pad_key(pad, LV_SYMBOL_LEFT, domain::NavKey::Left, on_key, context, kPadKey,
                             theme::kR4);
    lv_obj_set_pos(left, 2, kPadKeyMid);
    lv_obj_t* right = pad_key(pad, LV_SYMBOL_RIGHT, domain::NavKey::Right, on_key, context, kPadKey,
                              theme::kR4);
    lv_obj_set_pos(right, kPadSize - kPadKey - 2, kPadKeyMid);

    lv_obj_t* ok = lv_button_create(pad);
    lv_obj_remove_style_all(ok);
    lv_obj_set_size(ok, kPadOk, kPadOk);
    lv_obj_set_pos(ok, kPadOkPos, kPadOkPos);
    lv_obj_set_style_radius(ok, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ok, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ok, theme::accent(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ok, theme::accent_pressed(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(ok, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ok, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* ok_label = lv_label_create(ok);
    lv_obj_remove_style_all(ok_label);
    lv_label_set_text(ok_label, "OK");
    lv_obj_set_style_text_font(ok_label, theme::font_b15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ok_label, theme::accent_ink(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(ok_label, 1, LV_PART_MAIN);

    auto* binding = static_cast<KeyBinding*>(lv_malloc(sizeof(KeyBinding)));
    binding->handler = on_key;
    binding->context = context;
    binding->key = domain::NavKey::Ok;
    lv_obj_add_event_cb(
        ok,
        [](lv_event_t* event) {
            auto* data = static_cast<KeyBinding*>(lv_event_get_user_data(event));
            data->handler(data->context, data->key);
        },
        LV_EVENT_CLICKED, binding);
    lv_obj_add_event_cb(
        ok, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); }, LV_EVENT_DELETE,
        binding);
    return pad;
}

// ============================= kołyska =============================

namespace {

constexpr int32_t kRockerWidth = 72;
constexpr int32_t kRockerKey = 80;

// Dotknięcie, a przy `repeat` także przytrzymanie, woła `handler`.
void bind_press(lv_obj_t* button, bool repeat, PlainHandler handler, void* context) {
    Callback* callback = attach(button, reinterpret_cast<void*>(handler), context);
    auto press = [](lv_event_t* event) {
        auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
        reinterpret_cast<PlainHandler>(data->handler)(data->context);
    };
    // Przy powtarzaniu pierwszy krok idzie od razu po dotknięciu, kolejne co
    // interwał powtórzeń LVGL, a puszczenie niczego już nie dokłada.
    lv_obj_add_event_cb(button, press, repeat ? LV_EVENT_PRESSED : LV_EVENT_CLICKED, callback);
    if (repeat) {
        lv_obj_add_event_cb(button, press, LV_EVENT_LONG_PRESSED_REPEAT, callback);
    }
}

lv_obj_t* rocker_key(lv_obj_t* parent, const char* symbol, bool repeat, PlainHandler handler,
                     void* context) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, kRockerWidth - 4, kRockerKey);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, theme::dim(), LV_PART_MAIN);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    inherited_icon(button, symbol);
    bind_press(button, repeat, handler, context);
    return button;
}

}  // namespace

lv_obj_t* make_step_button(lv_obj_t* parent, const char* symbol, bool repeat,
                           PlainHandler on_press, void* context) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, 72, 72);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_text_color(button, theme::text(), LV_PART_MAIN);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    inherited_icon(button, symbol);
    bind_press(button, repeat, on_press, context);
    return button;
}

lv_obj_t* make_rocker(lv_obj_t* parent, const char* caption, bool repeat, PlainHandler on_up,
                      PlainHandler on_down, void* context) {
    lv_obj_t* rocker = plain(parent);
    lv_obj_set_size(rocker, kRockerWidth, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rocker, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rocker, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(rocker, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(rocker, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_radius(rocker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(rocker, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(rocker, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rocker, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(rocker, theme::kS1, LV_PART_MAIN);

    rocker_key(rocker, LV_SYMBOL_PLUS, repeat, on_up, context);

    lv_obj_t* label = lv_label_create(rocker);
    lv_obj_remove_style_all(label);
    lv_label_set_text(label, caption);
    lv_obj_set_style_text_font(label, theme::font_b15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, theme::faint(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, 1, LV_PART_MAIN);

    rocker_key(rocker, LV_SYMBOL_MINUS, repeat, on_down, context);
    return rocker;
}

// ============================= gładzik =============================

namespace {

// Pole jest kołem, więc strefę wyznacza odległość od środka i kąt, nie prostokąty.
struct Trackpad {
    lv_obj_t* root = nullptr;
    lv_obj_t* keys[4]{};   // góra, dół, lewo, prawo
    lv_obj_t* center = nullptr;
    int32_t acc_x = 0;
    int32_t acc_y = 0;
    int32_t travelled = 0;
    bool held = false;      // przytrzymany środek: puszczenie nie jest już stuknięciem
    KeyHandler handler = nullptr;
    KeyHandler hold_handler = nullptr;
    void* context = nullptr;
};

int key_slot(domain::NavKey key) {
    switch (key) {
        case domain::NavKey::Up: return 0;
        case domain::NavKey::Down: return 1;
        case domain::NavKey::Left: return 2;
        case domain::NavKey::Right: return 3;
        default: return -1;
    }
}

void trackpad_highlight(Trackpad* pad, domain::NavKey key) {
    lv_obj_t* target = nullptr;
    if (key == domain::NavKey::Ok) {
        target = pad->center;
    } else {
        const int slot = key_slot(key);
        if (slot >= 0) {
            target = pad->keys[slot];
        }
    }
    if (target != nullptr) {
        flash(target);
    }
}

domain::NavKey trackpad_zone(Trackpad* pad) {
    lv_point_t point{};
    lv_indev_get_point(lv_indev_active(), &point);
    lv_area_t area;
    lv_obj_get_coords(pad->root, &area);
    const float width = static_cast<float>(lv_area_get_width(&area));
    const float dx = static_cast<float>(point.x) - static_cast<float>(area.x1 + area.x2) / 2.0F;
    const float dy = static_cast<float>(point.y) - static_cast<float>(area.y1 + area.y2) / 2.0F;
    if (std::sqrt(dx * dx + dy * dy) < width * 0.22F) {
        return domain::NavKey::Ok;
    }
    const float angle = std::atan2(dy, dx) * 180.0F / 3.14159265F;
    if (angle >= -45.0F && angle < 45.0F) {
        return domain::NavKey::Right;
    }
    if (angle >= 45.0F && angle < 135.0F) {
        return domain::NavKey::Down;
    }
    if (angle >= -135.0F && angle < -45.0F) {
        return domain::NavKey::Up;
    }
    return domain::NavKey::Left;
}

void trackpad_event(lv_event_t* event) {
    auto* pad = static_cast<Trackpad*>(lv_event_get_user_data(event));
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        pad->acc_x = 0;
        pad->acc_y = 0;
        pad->travelled = 0;
        pad->held = false;
        return;
    }

    if (code == LV_EVENT_LONG_PRESSED) {
        // Przytrzymanie liczy się tylko w środku i bez przesunięcia palca —
        // dłuższy ruch po gładziku to przewijanie, nie gest.
        if (pad->hold_handler == nullptr || pad->travelled > 12 ||
            trackpad_zone(pad) != domain::NavKey::Ok) {
            return;
        }
        pad->held = true;
        trackpad_highlight(pad, domain::NavKey::Ok);
        pad->hold_handler(pad->context, domain::NavKey::Ok);
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_point_t vect{};
        lv_indev_get_vect(lv_indev_active(), &vect);
        pad->acc_x += vect.x;
        pad->acc_y += vect.y;
        pad->travelled += std::abs(vect.x) + std::abs(vect.y);
        // Po tylu pikselach wysyłamy kolejny krok (ustawienie czułości, domyślnie 26).
        const int32_t step = storage::get(storage::Pref::TrackpadStep);
        if (std::abs(pad->acc_x) < step && std::abs(pad->acc_y) < step) {
            return;
        }
        // Po każdym kroku liczymy od nowa, więc jeden długi ruch przewija
        // listę wielokrotnie.
        const domain::NavKey key =
            std::abs(pad->acc_x) > std::abs(pad->acc_y)
                ? (pad->acc_x > 0 ? domain::NavKey::Right : domain::NavKey::Left)
                : (pad->acc_y > 0 ? domain::NavKey::Down : domain::NavKey::Up);
        pad->acc_x = 0;
        pad->acc_y = 0;
        trackpad_highlight(pad, key);
        pad->handler(pad->context, key);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (pad->travelled > 12 || pad->held) {
            return;  // to był gest albo przytrzymanie, nie dotknięcie strefy
        }
        const domain::NavKey key = trackpad_zone(pad);
        trackpad_highlight(pad, key);
        pad->handler(pad->context, key);
    }
}

lv_obj_t* trackpad_marker(lv_obj_t* parent, const char* symbol, int32_t x, int32_t y) {
    lv_obj_t* marker = plain(parent);
    lv_obj_set_size(marker, kTrackpadKey, kTrackpadKey);
    lv_obj_set_pos(marker, x, y);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_remove_flag(marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(marker, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(marker, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_text_color(marker, theme::faint(), LV_PART_MAIN);
    inherited_icon(marker, symbol);
    return marker;
}

}  // namespace

lv_obj_t* make_trackpad(lv_obj_t* parent, KeyHandler on_key, void* context,
                        KeyHandler on_hold, int32_t size) {
    if (size <= 0) {
        size = kTrackpadSize;
    }
    // Środek rośnie razem z kołem, żeby OK nie zrobiło się w dużym kole za małe.
    const int32_t center = size * kTrackpadCenter / kTrackpadSize;
    auto* pad = static_cast<Trackpad*>(lv_malloc_zeroed(sizeof(Trackpad)));
    pad->handler = on_key;
    pad->hold_handler = on_hold;
    pad->context = context;

    pad->root = plain(parent);
    lv_obj_set_size(pad->root, size, size);
    lv_obj_set_style_bg_opa(pad->root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pad->root, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(pad->root, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(pad->root, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_radius(pad->root, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_add_flag(pad->root, LV_OBJ_FLAG_CLICKABLE);

    const int32_t mid = (size - kTrackpadKey) / 2;
    pad->keys[0] = trackpad_marker(pad->root, LV_SYMBOL_UP, mid, 5);
    pad->keys[1] = trackpad_marker(pad->root, LV_SYMBOL_DOWN, mid, size - kTrackpadKey - 5);
    pad->keys[2] = trackpad_marker(pad->root, LV_SYMBOL_LEFT, 5, mid);
    pad->keys[3] = trackpad_marker(pad->root, LV_SYMBOL_RIGHT, size - kTrackpadKey - 5, mid);

    // Powierzchnia jest jednolita: nie ma na niej wypełnionego przycisku, bo akcent
    // oznacza w tym systemie stan, a nie akcję.
    pad->center = plain(pad->root);
    lv_obj_set_size(pad->center, center, center);
    lv_obj_center(pad->center);
    lv_obj_set_style_radius(pad->center, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(pad->center, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(pad->center, theme::line(), LV_PART_MAIN);
    lv_obj_remove_flag(pad->center, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(pad->center, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pad->center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t* ok = make_cap(pad->center, "OK");
    lv_obj_set_style_text_color(ok, theme::faint(), LV_PART_MAIN);

    lv_obj_add_event_cb(pad->root, trackpad_event, LV_EVENT_PRESSED, pad);
    lv_obj_add_event_cb(pad->root, trackpad_event, LV_EVENT_PRESSING, pad);
    lv_obj_add_event_cb(pad->root, trackpad_event, LV_EVENT_RELEASED, pad);
    lv_obj_add_event_cb(pad->root, trackpad_event, LV_EVENT_LONG_PRESSED, pad);
    lv_obj_add_event_cb(
        pad->root, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, pad);
    return pad->root;
}

}  // namespace ui::widgets
