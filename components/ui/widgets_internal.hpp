#pragma once

// Pomocniki wspólne dla plików widżetów. Prywatne dla komponentu `ui`.

#include <algorithm>

#include "ui/widgets.hpp"

namespace ui::widgets::detail {

using theme::styles;

struct Callback {
    void* context = nullptr;
    void* handler = nullptr;
    std::size_t index = 0;
};

// Wywołania zwrotne żyją tak długo jak obiekt, więc wiążemy je z nim i oddajemy
// pamięć razem z nim.
inline Callback* attach(lv_obj_t* obj, void* handler, void* context, std::size_t index = 0) {
    auto* callback = static_cast<Callback*>(lv_malloc(sizeof(Callback)));
    callback->context = context;
    callback->handler = handler;
    callback->index = index;
    lv_obj_add_event_cb(
        obj, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, callback);
    return callback;
}

inline void center_text(lv_obj_t* label) {
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

inline lv_obj_t* plain(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    theme::make_flat(obj);
    return obj;
}

// Symbol dziedziczący kolor po rodzicu — używany tam, gdzie dotknięcie ma go zmienić.
inline lv_obj_t* inherited_icon(lv_obj_t* parent, const char* symbol) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    // Font przed tekstem: inaczej LVGL mierzy symbol krojem, który go nie zawiera.
    lv_obj_set_style_text_font(label, theme::font_icon(), LV_PART_MAIN);
    lv_label_set_text(label, symbol);
    return label;
}

inline lv_obj_t* icon_label(lv_obj_t* parent, const char* symbol, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    // Bez tego label o zadanej szerokości próbuje zawinąć glif, który się w niej
    // nie mieści, i LVGL zapętla łamanie wiersza.
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    // Font przed tekstem: inaczej LVGL mierzy symbol krojem, który go nie zawiera.
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_label_set_text(label, symbol);
    return label;
}

inline int32_t clamp100(int32_t value) {
    return std::clamp<int32_t>(value, 0, 100);
}

// Zakres 0–100 z krokiem 2 obowiązuje wszystkie wartości ciągłe Z906.
inline int32_t step2(int32_t value) {
    return (clamp100(value) + 1) / 2 * 2;
}

}  // namespace ui::widgets::detail
