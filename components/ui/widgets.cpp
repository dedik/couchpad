#include "widgets_internal.hpp"

#include <cstdio>
#include <cstring>

#include "core/config.hpp"

namespace ui::widgets {

using namespace detail;

namespace {

constexpr int32_t kSegItemHeight = 60;

}  // namespace

// ============================= sekcje =============================

lv_obj_t* make_section(lv_obj_t* parent) {
    lv_obj_t* section = plain(parent);
    lv_obj_set_size(section, LV_PCT(100), LV_PCT(100));
    theme::make_column(section, theme::kS3);
    return section;
}

lv_obj_t* make_fill(lv_obj_t* parent) {
    lv_obj_t* fill = plain(parent);
    lv_obj_set_width(fill, LV_PCT(100));
    lv_obj_set_flex_grow(fill, 1);
    lv_obj_set_flex_flow(fill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return fill;
}

lv_obj_t* make_cap(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_add_style(label, &styles().cap, LV_PART_MAIN);
    lv_label_set_text(label, text);
    return label;
}

// ============================= grupa wyboru =============================

void Segmented::set_options(const domain::OptionList& options) {
    for (std::size_t i = 0; i < count; ++i) {
        if (i < options.size()) {
            lv_obj_remove_flag(items[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lv_obj_get_child(items[i], 0), options[i].c_str());
        } else {
            lv_obj_add_flag(items[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Segmented::set_labels(const char* const* labels, std::size_t labels_count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (i < labels_count) {
            lv_obj_remove_flag(items[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lv_obj_get_child(items[i], 0), labels[i]);
        } else {
            lv_obj_add_flag(items[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Segmented::set_selected(std::size_t index) {
    for (std::size_t i = 0; i < count; ++i) {
        const bool selected = i == index;
        lv_obj_remove_style(items[i], &styles().seg_item_sel, LV_PART_MAIN);
        if (selected) {
            lv_obj_add_style(items[i], &styles().seg_item_sel, LV_PART_MAIN);
        }
    }
}

void Segmented::set_selected_none() {
    set_selected(domain::kMaxOptions + 1);
}

void Segmented::set_status(std::size_t index, domain::ControlStatus status) {
    for (std::size_t i = 0; i < count; ++i) {
        set_status_outline(items[i], i == index ? status : domain::ControlStatus::Idle);
    }
}

const char* Segmented::label_at(std::size_t index) const {
    if (index >= count) {
        return "";
    }
    return lv_label_get_text(lv_obj_get_child(items[index], 0));
}

Segmented make_segmented(lv_obj_t* parent, const char* leading_symbol, std::size_t slots,
                         IndexHandler on_select, void* context) {
    Segmented segmented;
    segmented.count = std::min(slots, domain::kMaxOptions);

    segmented.root = plain(parent);
    lv_obj_set_width(segmented.root, LV_PCT(100));
    lv_obj_set_height(segmented.root, LV_SIZE_CONTENT);
    theme::make_row(segmented.root, theme::kS3);

    lv_obj_t* lead = icon_label(segmented.root, leading_symbol, theme::font_icon_sm(), theme::faint());
    lv_obj_set_width(lead, 24);
    center_text(lead);

    segmented.box = lv_obj_create(segmented.root);
    lv_obj_remove_style_all(segmented.box);
    lv_obj_add_style(segmented.box, &styles().group_box, LV_PART_MAIN);
    lv_obj_remove_flag(segmented.box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(segmented.box, kSegItemHeight + 2 * theme::kS1);
    lv_obj_set_flex_grow(segmented.box, 1);
    theme::make_row(segmented.box, theme::kS1);

    for (std::size_t i = 0; i < segmented.count; ++i) {
        lv_obj_t* item = lv_button_create(segmented.box);
        lv_obj_remove_style_all(item);
        lv_obj_add_style(item, &styles().seg_item, LV_PART_MAIN);
        lv_obj_add_style(item, &styles().pressed_soft, LV_STATE_PRESSED);
        lv_obj_set_height(item, kSegItemHeight);
        lv_obj_set_flex_grow(item, 1);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* label = lv_label_create(item);
        lv_obj_remove_style_all(label);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        center_text(label);
        lv_label_set_text(label, "");

        Callback* callback = attach(item, reinterpret_cast<void*>(on_select), context, i);
        lv_obj_add_event_cb(
            item,
            [](lv_event_t* event) {
                auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
                reinterpret_cast<IndexHandler>(data->handler)(data->context, data->index);
            },
            LV_EVENT_CLICKED, callback);

        segmented.items[i] = item;
    }
    return segmented;
}

Segmented make_value_segment(lv_obj_t* row, const char* leading_symbol,
                             IndexHandler on_press, void* context) {
    Segmented segmented = make_segmented(row, leading_symbol, 1, on_press, context);
    lv_obj_set_width(segmented.root, 0);
    lv_obj_set_flex_grow(segmented.root, 1);
    lv_obj_t* label = lv_obj_get_child(segmented.items[0], 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return segmented;
}

// ============================= kafelki =============================

void Tiles::set_selected(std::size_t index) {
    for (std::size_t i = 0; i < count; ++i) {
        lv_obj_remove_style(items[i], &styles().tile_sel, LV_PART_MAIN);
        if (i == index) {
            lv_obj_add_style(items[i], &styles().tile_sel, LV_PART_MAIN);
        }
    }
}

void Tiles::set_selected_none() {
    set_selected(static_cast<std::size_t>(-1));
}

void Tiles::set_status(std::size_t index, domain::ControlStatus status) {
    for (std::size_t i = 0; i < count; ++i) {
        set_status_outline(items[i], i == index ? status : domain::ControlStatus::Idle);
    }
}

Tiles make_tiles(lv_obj_t* parent, const TileSpec* specs, std::size_t count, int32_t columns,
                 int32_t height, IndexHandler on_press, void* context) {
    Tiles tiles;
    tiles.count = std::min<std::size_t>(count, 6);

    const int32_t rows = static_cast<int32_t>((tiles.count + columns - 1) / columns);
    tiles.root = plain(parent);
    lv_obj_set_width(tiles.root, LV_PCT(100));
    lv_obj_set_height(tiles.root, rows * height + (rows - 1) * theme::kS2);
    lv_obj_set_layout(tiles.root, LV_LAYOUT_GRID);

    // LVGL trzyma wskaźnik do deskryptorów, więc muszą żyć tak długo jak obiekt
    // i nie mogą być wspólne dla kilku siatek.
    auto* desc = static_cast<int32_t*>(lv_malloc(sizeof(int32_t) * (columns + rows + 2)));
    int32_t* col_desc = desc;
    int32_t* row_desc = desc + columns + 1;
    for (int32_t i = 0; i < columns; ++i) {
        col_desc[i] = LV_GRID_FR(1);
    }
    col_desc[columns] = LV_GRID_TEMPLATE_LAST;
    for (int32_t i = 0; i < rows; ++i) {
        row_desc[i] = LV_GRID_FR(1);
    }
    row_desc[rows] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(tiles.root, col_desc, row_desc);
    lv_obj_add_event_cb(
        tiles.root, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, desc);
    lv_obj_set_style_pad_column(tiles.root, theme::kS2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tiles.root, theme::kS2, LV_PART_MAIN);

    for (std::size_t i = 0; i < tiles.count; ++i) {
        lv_obj_t* tile = lv_button_create(tiles.root);
        lv_obj_remove_style_all(tile);
        lv_obj_add_style(tile, &styles().tile, LV_PART_MAIN);
        lv_obj_add_style(tile, &styles().pressed_soft, LV_STATE_PRESSED);
        lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(i) % columns, 1,
                             LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(i) / columns, 1);
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        if (specs[i].image != nullptr) {
            lv_obj_t* image = lv_image_create(tile);
            lv_image_set_src(image, specs[i].image);
        } else if (specs[i].symbol != nullptr) {
            icon_label(tile, specs[i].symbol, theme::font_icon(), theme::dim());
        }
        if (specs[i].text != nullptr) {
            lv_obj_t* label = lv_label_create(tile);
            lv_obj_remove_style_all(label);
            lv_label_set_text(label, specs[i].text);
            center_text(label);
        }

        Callback* callback = attach(tile, reinterpret_cast<void*>(on_press), context, i);
        lv_obj_add_event_cb(
            tile,
            [](lv_event_t* event) {
                auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
                reinterpret_cast<IndexHandler>(data->handler)(data->context, data->index);
            },
            LV_EVENT_CLICKED, callback);

        tiles.items[i] = tile;
    }
    return tiles;
}


// ============================= akcje =============================

lv_obj_t* make_action(lv_obj_t* parent, const char* symbol, const char* text, bool primary,
                      ActionLayout layout, PlainHandler on_press, void* context) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles().act, LV_PART_MAIN);
    if (primary) {
        lv_obj_add_style(button, &styles().act_primary, LV_PART_MAIN);
    } else {
        lv_obj_add_style(button, &styles().pressed_soft, LV_STATE_PRESSED);
    }
    lv_obj_set_flex_flow(button,
                         layout == ActionLayout::Stacked ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    switch (layout) {
        case ActionLayout::Stacked:
            lv_obj_set_size(button, 84, 124);
            lv_obj_set_style_text_font(button, theme::font_r13(), LV_PART_MAIN);
            break;
        case ActionLayout::Fill:
            lv_obj_set_height(button, 72);
            lv_obj_set_flex_grow(button, 1);
            break;
        case ActionLayout::Block:
            lv_obj_set_size(button, LV_PCT(100), 72);
            break;
    }

    if (symbol != nullptr) {
        icon_label(button, symbol, theme::font_icon_sm(),
                   primary ? theme::accent_ink() : theme::dim());
    }

    if (text != nullptr) {
        lv_obj_t* label = lv_label_create(button);
        lv_obj_remove_style_all(label);
        lv_label_set_text(label, text);
        center_text(label);
    }

    if (on_press != nullptr) {
        Callback* callback = attach(button, reinterpret_cast<void*>(on_press), context);
        lv_obj_add_event_cb(
            button,
            [](lv_event_t* event) {
                auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
                reinterpret_cast<PlainHandler>(data->handler)(data->context);
            },
            LV_EVENT_CLICKED, callback);
    }
    return button;
}

namespace {

using core::config::kDoubleTapMs;

struct Presses {
    PressHandler handler = nullptr;
    void* context = nullptr;
    lv_timer_t* pending = nullptr;   // czeka, czy przyjdzie drugie stuknięcie
};

void presses_single(lv_timer_t* timer) {
    auto* presses = static_cast<Presses*>(lv_timer_get_user_data(timer));
    presses->pending = nullptr;
    presses->handler(presses->context, domain::Press::Tap);
}

void presses_event(lv_event_t* event) {
    auto* presses = static_cast<Presses*>(lv_event_get_user_data(event));
    switch (lv_event_get_code(event)) {
        case LV_EVENT_SHORT_CLICKED:
            if (presses->pending != nullptr) {
                lv_timer_delete(presses->pending);
                presses->pending = nullptr;
                presses->handler(presses->context, domain::Press::Double);
                return;
            }
            presses->pending = lv_timer_create(presses_single, kDoubleTapMs, presses);
            lv_timer_set_repeat_count(presses->pending, 1);
            return;
        case LV_EVENT_LONG_PRESSED:
            // Przytrzymanie po pierwszym stuknięciu zamyka okno podwójnego.
            if (presses->pending != nullptr) {
                lv_timer_delete(presses->pending);
                presses->pending = nullptr;
            }
            presses->handler(presses->context, domain::Press::Hold);
            return;
        case LV_EVENT_DELETE:
            if (presses->pending != nullptr) {
                lv_timer_delete(presses->pending);
            }
            lv_free(presses);
            return;
        default:
            return;
    }
}

}  // namespace

void attach_presses(lv_obj_t* button, PressHandler handler, void* context) {
    auto* presses = static_cast<Presses*>(lv_malloc_zeroed(sizeof(Presses)));
    presses->handler = handler;
    presses->context = context;
    lv_obj_add_event_cb(button, presses_event, LV_EVENT_SHORT_CLICKED, presses);
    lv_obj_add_event_cb(button, presses_event, LV_EVENT_LONG_PRESSED, presses);
    lv_obj_add_event_cb(button, presses_event, LV_EVENT_DELETE, presses);
}

lv_obj_t* make_duo(lv_obj_t* parent) {
    lv_obj_t* duo = plain(parent);
    lv_obj_set_width(duo, LV_PCT(100));
    lv_obj_set_height(duo, 72);
    theme::make_row(duo, theme::kS2);
    return duo;
}

// ============================= wiersz listy =============================

ListRow make_list_row(lv_obj_t* parent, const char* symbol, const char* title, const char* note,
                      const char* value, PlainHandler on_press, void* context) {
    ListRow row;
    row.root = lv_button_create(parent);
    lv_obj_remove_style_all(row.root);
    lv_obj_add_style(row.root, &styles().list_row, LV_PART_MAIN);
    lv_obj_add_style(row.root, &styles().pressed_soft, LV_STATE_PRESSED);
    lv_obj_set_width(row.root, LV_PCT(100));
    lv_obj_set_height(row.root, 76);
    theme::make_row(row.root, theme::kS3);

    lv_obj_t* lead = icon_label(row.root, symbol, theme::font_icon_sm(), theme::dim());
    lv_obj_set_width(lead, 28);
    center_text(lead);

    lv_obj_t* texts = plain(row.root);
    // Kontener tekstów nie może łykać dotknięcia: wiersz ma reagować na całej
    // szerokości, nie tylko na strzałce.
    lv_obj_remove_flag(texts, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(texts, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(texts, 1);
    theme::make_column(texts, 2);

    row.title = lv_label_create(texts);
    lv_obj_remove_style_all(row.title);
    lv_obj_set_style_text_font(row.title, theme::font_m15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(row.title, theme::text(), LV_PART_MAIN);
    lv_label_set_text(row.title, title);

    row.note = lv_label_create(texts);
    lv_obj_remove_style_all(row.note);
    lv_obj_add_style(row.note, &styles().cap, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(row.note, 0, LV_PART_MAIN);
    lv_label_set_text(row.note, note != nullptr ? note : "");

    row.value = lv_label_create(row.root);
    lv_obj_remove_style_all(row.value);
    lv_obj_set_style_text_font(row.value, theme::font_r13(), LV_PART_MAIN);
    lv_obj_set_style_text_color(row.value, theme::dim(), LV_PART_MAIN);
    lv_label_set_text(row.value, value != nullptr ? value : "");

    icon_label(row.root, LV_SYMBOL_RIGHT, theme::font_icon_sm(), theme::faint());

    if (on_press != nullptr) {
        Callback* callback = attach(row.root, reinterpret_cast<void*>(on_press), context);
        lv_obj_add_event_cb(
            row.root,
            [](lv_event_t* event) {
                auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
                reinterpret_cast<PlainHandler>(data->handler)(data->context);
            },
            LV_EVENT_CLICKED, callback);
    }
    return row;
}

// ============================= wiersz informacyjny =============================

void Readout::set(const char* text, bool on) {
    lv_label_set_text(value, text);
    lv_obj_set_style_text_color(value, on ? theme::ok() : theme::dim(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, on ? theme::ok() : theme::faint(), LV_PART_MAIN);
}

Readout make_readout(lv_obj_t* parent, const char* name) {
    Readout readout;
    readout.root = plain(parent);
    lv_obj_set_width(readout.root, LV_PCT(100));
    lv_obj_set_height(readout.root, 44);
    theme::make_row(readout.root, theme::kS2);
    lv_obj_set_style_pad_hor(readout.root, theme::kS1, LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(readout.root);
    lv_obj_remove_style_all(title);
    lv_obj_set_style_text_font(title, theme::font_r13(), LV_PART_MAIN);
    lv_obj_set_style_text_color(title, theme::faint(), LV_PART_MAIN);
    lv_label_set_text(title, name);
    lv_obj_set_flex_grow(title, 1);

    readout.dot = plain(readout.root);
    lv_obj_set_size(readout.dot, 8, 8);
    lv_obj_set_style_radius(readout.dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(readout.dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(readout.dot, theme::faint(), LV_PART_MAIN);

    readout.value = lv_label_create(readout.root);
    lv_obj_remove_style_all(readout.value);
    lv_obj_set_style_text_font(readout.value, theme::font_m13(), LV_PART_MAIN);
    lv_obj_set_style_text_color(readout.value, theme::dim(), LV_PART_MAIN);
    lv_label_set_text(readout.value, "");
    return readout;
}

// ============================= pasek braku stanu =============================

Banner make_banner(lv_obj_t* parent, const char* text) {
    Banner banner;

    // Warstwa łykająca dotknięcia. Bez niej przygaszone sterowanie nadal da się
    // nacisnąć, a pilot wysłałby polecenie w oparciu o nieaktualny stan.
    banner.blocker = plain(parent);
    lv_obj_add_flag(banner.blocker, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(banner.blocker, LV_PCT(100), LV_PCT(100));
    lv_obj_align(banner.blocker, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(banner.blocker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(banner.blocker, LV_OBJ_FLAG_HIDDEN);

    banner.box = plain(parent);
    lv_obj_remove_flag(banner.box, LV_OBJ_FLAG_CLICKABLE);
    // Leży poza układem rodzica, więc jego pojawienie się nic nie przesuwa.
    lv_obj_add_flag(banner.box, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_width(banner.box, theme::kContentW);
    lv_obj_set_height(banner.box, LV_SIZE_CONTENT);
    lv_obj_align(banner.box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(banner.box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(banner.box, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(banner.box, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(banner.box, theme::danger(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(banner.box, 90, LV_PART_MAIN);
    lv_obj_set_style_radius(banner.box, theme::kR4, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(banner.box, theme::kS5, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(banner.box, theme::kS6, LV_PART_MAIN);
    theme::make_column(banner.box, theme::kS3);
    lv_obj_set_flex_align(banner.box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    icon_label(banner.box, LV_SYMBOL_WARNING, theme::font_icon(), theme::danger());

    lv_obj_t* label = lv_label_create(banner.box);
    lv_obj_remove_style_all(label);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    center_text(label);
    lv_obj_set_style_text_font(label, theme::font_r13(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(label, 5, LV_PART_MAIN);

    lv_obj_add_flag(banner.box, LV_OBJ_FLAG_HIDDEN);
    return banner;
}

void set_unavailable(lv_obj_t* section, const Banner& banner, bool unavailable) {
    const uint32_t children = lv_obj_get_child_count(section);
    for (uint32_t i = 0; i < children; ++i) {
        lv_obj_t* child = lv_obj_get_child(section, i);
        if (child == banner.box || child == banner.blocker) {
            continue;
        }
        // Zamiast zgadywać wartości, gasimy sterowanie i mówimy wprost, czego brakuje.
        lv_obj_set_style_opa(child, unavailable ? 77 : LV_OPA_COVER, LV_PART_MAIN);
    }
    if (unavailable) {
        lv_obj_remove_flag(banner.blocker, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(banner.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(banner.blocker);
        lv_obj_move_foreground(banner.box);
    } else {
        lv_obj_add_flag(banner.blocker, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(banner.box, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================= zasilanie =============================

lv_obj_t* make_power_button(lv_obj_t* parent, PlainHandler on_press, void* context) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, 64, 64);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon_label(button, LV_SYMBOL_POWER, theme::font_icon(), theme::dim());

    Callback* callback = attach(button, reinterpret_cast<void*>(on_press), context);
    lv_obj_add_event_cb(
        button,
        [](lv_event_t* event) {
            auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
            reinterpret_cast<PlainHandler>(data->handler)(data->context);
        },
        LV_EVENT_CLICKED, callback);
    return button;
}

void set_power_state(lv_obj_t* button, bool on, bool enabled) {
    lv_obj_set_style_bg_color(button, on ? theme::accent() : theme::surface2(), LV_PART_MAIN);
    lv_obj_t* icon = lv_obj_get_child(button, 0);
    lv_obj_set_style_text_color(icon, on ? theme::accent_ink() : theme::dim(), LV_PART_MAIN);
    lv_obj_set_style_opa(button, enabled ? LV_OPA_COVER : 77, LV_PART_MAIN);
    if (enabled) {
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(button, LV_OBJ_FLAG_CLICKABLE);
    }
}

// ============================= podstrona =============================

Page make_page(const char* title, PlainHandler on_back, void* context) {
    Page page;

    // Warstwa systemowa leży nad `lv_layer_top`, na której siedzi nakładka
    // wybudzania. Bez tego nakładka zasłaniałaby ustawienia przy każdym
    // odświeżeniu — a przy pierwszym uruchomieniu to jedyne wyjście z niej.
    page.root = plain(lv_layer_sys());
    lv_obj_set_size(page.root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(page.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(page.root, theme::bg(), LV_PART_MAIN);
    lv_obj_add_flag(page.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(page.root, LV_OBJ_FLAG_HIDDEN);
    theme::make_column(page.root, 0);

    lv_obj_t* bar = plain(page.root);
    lv_obj_set_size(bar, LV_PCT(100), theme::kTopbar);
    lv_obj_set_style_pad_hor(bar, theme::kGutter, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, theme::kTopbarPad, LV_PART_MAIN);
    theme::make_row(bar, theme::kS3);

    lv_obj_t* back = lv_button_create(bar);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 56, 56);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_text_color(back, theme::dim(), LV_PART_MAIN);
    lv_obj_add_style(back, &styles().pressed_soft, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(back, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    inherited_icon(back, LV_SYMBOL_LEFT);

    page.title = lv_label_create(bar);
    lv_obj_remove_style_all(page.title);
    lv_label_set_text(page.title, title);
    lv_obj_set_style_text_font(page.title, theme::font_b19(), LV_PART_MAIN);
    lv_obj_set_style_text_color(page.title, theme::text(), LV_PART_MAIN);
    lv_obj_set_flex_grow(page.title, 1);

    page.body = plain(page.root);
    lv_obj_set_width(page.body, LV_PCT(100));
    lv_obj_set_flex_grow(page.body, 1);
    lv_obj_set_style_pad_hor(page.body, theme::kGutter, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(page.body, theme::kS4, LV_PART_MAIN);
    theme::make_column(page.body, theme::kS3);
    // Treść dłuższa niż ekran przewija się w pionie. Bez tego dolne wiersze
    // (np. „Źródło stanu” na stronie MQTT) leżały poza ekranem, nieosiągalne.
    // Pasek przewijania pokazuje się w trakcie ruchu, żeby było widać, że jest dalej.
    lv_obj_add_flag(page.body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page.body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page.body, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_width(page.body, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(page.body, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(page.body, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(page.body, theme::faint(), LV_PART_SCROLLBAR);

    if (on_back != nullptr) {
        Callback* callback = attach(back, reinterpret_cast<void*>(on_back), context);
        lv_obj_add_event_cb(
            back,
            [](lv_event_t* event) {
                auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
                reinterpret_cast<PlainHandler>(data->handler)(data->context);
            },
            LV_EVENT_CLICKED, callback);
    }
    return page;
}

void show_page(const Page& page, bool visible) {
    if (visible) {
        lv_obj_remove_flag(page.root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(page.root);
    } else {
        lv_obj_add_flag(page.root, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================= pytanie o potwierdzenie =============================

namespace {

struct Confirm {
    lv_obj_t* scrim = nullptr;
    lv_obj_t* box = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* message = nullptr;
    lv_obj_t* accept_label = nullptr;
    PlainHandler on_confirm = nullptr;
    void* context = nullptr;
};

Confirm g_confirm;

void close_confirm() {
    lv_obj_add_flag(g_confirm.scrim, LV_OBJ_FLAG_HIDDEN);
    g_confirm.on_confirm = nullptr;
    g_confirm.context = nullptr;
}

void on_confirm_cancel(void*) { close_confirm(); }

void on_confirm_accept(void*) {
    // Najpierw zamknięcie: obsługa może otworzyć kolejne okno albo przerysować ekran.
    const PlainHandler handler = g_confirm.on_confirm;
    void* context = g_confirm.context;
    close_confirm();
    if (handler != nullptr) {
        handler(context);
    }
}

// Budowane przy pierwszym użyciu: większość uruchomień nie pyta o nic.
void build_confirm() {
    g_confirm.scrim = plain(lv_layer_sys());
    lv_obj_set_size(g_confirm.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(g_confirm.scrim, 158, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_confirm.scrim, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_flag(g_confirm.scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        g_confirm.scrim,
        [](lv_event_t* event) {
            // Dotknięcie tła, nie okna: okno przechwytuje własne dotknięcia.
            if (lv_event_get_target(event) == g_confirm.scrim) {
                close_confirm();
            }
        },
        LV_EVENT_CLICKED, nullptr);

    g_confirm.box = plain(g_confirm.scrim);
    lv_obj_set_width(g_confirm.box, LV_PCT(100));
    lv_obj_set_height(g_confirm.box, LV_SIZE_CONTENT);
    lv_obj_set_style_margin_hor(g_confirm.box, theme::kS4, LV_PART_MAIN);
    lv_obj_align(g_confirm.box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(g_confirm.box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_confirm.box, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_confirm.box, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_confirm.box, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_radius(g_confirm.box, theme::kR4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_confirm.box, theme::kS5, LV_PART_MAIN);
    lv_obj_add_flag(g_confirm.box, LV_OBJ_FLAG_CLICKABLE);
    theme::make_column(g_confirm.box, theme::kS4);

    g_confirm.title = lv_label_create(g_confirm.box);
    lv_obj_remove_style_all(g_confirm.title);
    lv_label_set_long_mode(g_confirm.title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_confirm.title, LV_PCT(100));
    lv_obj_set_style_text_font(g_confirm.title, theme::font_b19(), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_confirm.title, theme::text(), LV_PART_MAIN);

    g_confirm.message = lv_label_create(g_confirm.box);
    lv_obj_remove_style_all(g_confirm.message);
    lv_label_set_long_mode(g_confirm.message, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_confirm.message, LV_PCT(100));
    lv_obj_set_style_text_font(g_confirm.message, theme::font_r15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_confirm.message, theme::dim(), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(g_confirm.message, 5, LV_PART_MAIN);

    lv_obj_t* duo = make_duo(g_confirm.box);
    make_action(duo, nullptr, "Anuluj", false, ActionLayout::Fill, &on_confirm_cancel, nullptr);
    lv_obj_t* accept =
        make_action(duo, nullptr, "", true, ActionLayout::Fill, &on_confirm_accept, nullptr);
    g_confirm.accept_label = lv_obj_get_child(accept, 0);
}

}  // namespace

void ask_confirm(const char* title, const char* message, const char* confirm_label,
                 PlainHandler on_confirm, void* context) {
    if (g_confirm.scrim == nullptr) {
        build_confirm();
    }
    g_confirm.on_confirm = on_confirm;
    g_confirm.context = context;
    lv_label_set_text(g_confirm.title, title);
    lv_label_set_text(g_confirm.message, message);
    lv_label_set_text(g_confirm.accept_label, confirm_label);
    lv_obj_remove_flag(g_confirm.scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_confirm.scrim);
}

void dismiss_confirm() {
    if (g_confirm.scrim != nullptr) {
        close_confirm();
    }
}

// ============================= narzędzia =============================

void set_status_outline(lv_obj_t* obj, domain::ControlStatus status) {
    lv_obj_remove_style(obj, &styles().pending, LV_PART_MAIN);
    lv_obj_remove_style(obj, &styles().failed, LV_PART_MAIN);
    if (status == domain::ControlStatus::Pending) {
        lv_obj_add_style(obj, &styles().pending, LV_PART_MAIN);
    } else if (status == domain::ControlStatus::Failed) {
        lv_obj_add_style(obj, &styles().failed, LV_PART_MAIN);
    }
}

void flash(lv_obj_t* obj) {
    lv_obj_set_style_bg_opa(obj, theme::kAccentSoftOpa, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, theme::accent(), LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, theme::accent(), LV_PART_MAIN);
    lv_timer_t* timer = lv_timer_create(
        [](lv_timer_t* self) {
            auto* target = static_cast<lv_obj_t*>(lv_timer_get_user_data(self));
            lv_obj_set_style_bg_opa(target, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(target, theme::faint(), LV_PART_MAIN);
        },
        150, obj);
    lv_timer_set_repeat_count(timer, 1);
}

}  // namespace ui::widgets
