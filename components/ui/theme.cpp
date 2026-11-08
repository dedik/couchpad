#include "ui/theme.hpp"

#include <cstddef>
#include <initializer_list>

#include "lvgl_private.h"

extern "C" {
LV_FONT_DECLARE(ubuntu_r_11)
LV_FONT_DECLARE(ubuntu_r_13)
LV_FONT_DECLARE(ubuntu_r_15)
LV_FONT_DECLARE(ubuntu_m_11)
LV_FONT_DECLARE(ubuntu_m_13)
LV_FONT_DECLARE(ubuntu_m_15)
LV_FONT_DECLARE(ubuntu_b_15)
LV_FONT_DECLARE(ubuntu_b_19)
LV_FONT_DECLARE(ubuntu_b_26)
LV_FONT_DECLARE(ubuntu_b_46_num)
LV_FONT_DECLARE(ubuntu_b_64_num)
LV_FONT_DECLARE(ubuntu_b_96_num)
LV_FONT_DECLARE(lucide_modes_24)
LV_FONT_DECLARE(lucide_effects_22)
LV_FONT_DECLARE(lucide_effects_36)
LV_FONT_DECLARE(lucide_modes_36)
}

namespace ui::theme {
namespace {

// Wartości ciemnej palety pochodzą z pulpit.css. Jasna ma te same role i zachowuje
// kontrast tekstu: akcent, zieleń i czerwień są ciemniejsze, bo leżą na bieli.
// Żadne dwie role jednej palety nie mają tego samego koloru — `apply()` rozpoznaje
// rolę po wartości.
const Palette kDark = {
    .bg = lv_color_hex(0x0a0b0e),
    .surface = lv_color_hex(0x14161b),
    .surface2 = lv_color_hex(0x1c1f26),
    .surface3 = lv_color_hex(0x282c35),
    .line = lv_color_hex(0x30353f),
    .text = lv_color_hex(0xf3f5f9),
    .dim = lv_color_hex(0x98a0ae),
    .faint = lv_color_hex(0x626a78),
    .accent = lv_color_hex(0xf2a93b),
    .accent_pressed = lv_color_hex(0xf4b25a),
    .accent_ink = lv_color_hex(0x17120a),
    .ok = lv_color_hex(0x56d6a0),
    .danger = lv_color_hex(0xff5f5f),
};

const Palette kLight = {
    .bg = lv_color_hex(0xeef0f3),
    .surface = lv_color_hex(0xffffff),
    .surface2 = lv_color_hex(0xe4e7ec),
    .surface3 = lv_color_hex(0xcfd4dc),
    .line = lv_color_hex(0xd9dde4),
    .text = lv_color_hex(0x15171c),
    .dim = lv_color_hex(0x586070),
    .faint = lv_color_hex(0x878f9d),
    .accent = lv_color_hex(0xe08a12),
    .accent_pressed = lv_color_hex(0xb86f0a),
    .accent_ink = lv_color_hex(0x1a1206),
    .ok = lv_color_hex(0x1c9a68),
    .danger = lv_color_hex(0xe03a3a),
};

constexpr std::size_t kRoles = sizeof(Palette) / sizeof(lv_color_t);
static_assert(sizeof(Palette) == kRoles * sizeof(lv_color_t), "paleta to same kolory");

// Właściwości stylu niosące kolor. Wszystkie, których używa LVGL 9.2.
constexpr lv_style_prop_t kColorProps[] = {
    LV_STYLE_BG_COLOR,     LV_STYLE_BG_GRAD_COLOR, LV_STYLE_BG_IMAGE_RECOLOR,
    LV_STYLE_BORDER_COLOR, LV_STYLE_OUTLINE_COLOR, LV_STYLE_SHADOW_COLOR,
    LV_STYLE_IMAGE_RECOLOR, LV_STYLE_LINE_COLOR,   LV_STYLE_ARC_COLOR,
    LV_STYLE_TEXT_COLOR,
};

Mode g_mode = Mode::Dark;
const Palette* g_palette = &kDark;

Styles g_styles;
bool g_ready = false;

const lv_color_t* roles(const Palette& palette) {
    return reinterpret_cast<const lv_color_t*>(&palette);
}

// Zamienia w stylu każdy kolor roli ze starej palety na tę samą rolę z nowej.
// Kolory spoza palety (czerń zasłony, obrazy) zostają.
void remap(lv_style_t* style, const Palette& from, const Palette& to) {
    const lv_color_t* old_roles = roles(from);
    const lv_color_t* new_roles = roles(to);
    for (lv_style_prop_t prop : kColorProps) {
        lv_style_value_t value;
        if (lv_style_get_prop(style, prop, &value) != LV_STYLE_RES_FOUND) {
            continue;
        }
        for (std::size_t i = 0; i < kRoles; ++i) {
            if (lv_color_eq(value.color, old_roles[i])) {
                value.color = new_roles[i];
                lv_style_set_prop(style, prop, value);
                break;
            }
        }
    }
}

struct RemapWalk {
    const Palette* from;
    const Palette* to;
};

// Style wspólne poprawia `apply()` raz; tu tylko lokalne, których każdy obiekt ma własne.
lv_obj_tree_walk_res_t remap_object(lv_obj_t* obj, void* raw) {
    const auto& walk = *static_cast<RemapWalk*>(raw);
    for (uint32_t i = 0; i < obj->style_cnt; ++i) {
        lv_obj_style_t& entry = obj->styles[i];
        if (entry.is_local) {
            remap(const_cast<lv_style_t*>(entry.style), *walk.from, *walk.to);
        }
    }
    return LV_OBJ_TREE_WALK_NEXT;
}

// Kroje klawiatury: Ubuntu z zapasowym Montserratem, żeby ⇧ i ⌫ (symbole LVGL)
// mogły leżeć w tej samej macierzy co litery. Kopie, bo wygenerowane kroje są stałe.
lv_font_t g_key_b19;
lv_font_t g_key_b26;

void reset_box(lv_style_t* style) {
    lv_style_init(style);
    lv_style_set_bg_opa(style, LV_OPA_TRANSP);
    lv_style_set_border_width(style, 0);
    lv_style_set_outline_width(style, 0);
    lv_style_set_pad_all(style, 0);
    lv_style_set_pad_gap(style, 0);
    lv_style_set_radius(style, 0);
}

}  // namespace

const lv_font_t* font_r11() { return &ubuntu_r_11; }
const lv_font_t* font_r13() { return &ubuntu_r_13; }
const lv_font_t* font_r15() { return &ubuntu_r_15; }
const lv_font_t* font_m11() { return &ubuntu_m_11; }
const lv_font_t* font_m13() { return &ubuntu_m_13; }
const lv_font_t* font_m15() { return &ubuntu_m_15; }
const lv_font_t* font_b15() { return &ubuntu_b_15; }
const lv_font_t* font_b19() { return &ubuntu_b_19; }
const lv_font_t* font_b26() { return &ubuntu_b_26; }
const lv_font_t* font_b46num() { return &ubuntu_b_46_num; }
const lv_font_t* font_b64num() { return &ubuntu_b_64_num; }
const lv_font_t* font_b96num() { return &ubuntu_b_96_num; }
const lv_font_t* font_key_b19() { return &g_key_b19; }
const lv_font_t* font_key_b26() { return &g_key_b26; }
const lv_font_t* font_icon() { return &lv_font_montserrat_24; }
const lv_font_t* font_mode_icon() { return &lucide_modes_24; }
const lv_font_t* font_effect_icon() { return &lucide_effects_22; }
const lv_font_t* font_effect_icon_lg() { return &lucide_effects_36; }
const lv_font_t* font_mode_icon_lg() { return &lucide_modes_36; }
const lv_font_t* font_icon_sm() { return &lv_font_montserrat_20; }
const lv_font_t* font_icon_xs() { return &lv_font_montserrat_14; }

const Styles& styles() { return g_styles; }
const Palette& palette() { return *g_palette; }
Mode mode() { return g_mode; }

void apply(Mode mode) {
    if (mode == g_mode) {
        return;
    }
    const Palette& from = *g_palette;
    const Palette& to = mode == Mode::Light ? kLight : kDark;
    g_mode = mode;
    g_palette = &to;
    if (!g_ready) {
        return;  // style powstaną już z nowej palety
    }

    lv_style_t* shared = reinterpret_cast<lv_style_t*>(&g_styles);
    for (std::size_t i = 0; i < sizeof(Styles) / sizeof(lv_style_t); ++i) {
        remap(&shared[i], from, to);
    }
    RemapWalk walk{&from, &to};
    for (lv_obj_t* root : {lv_layer_bottom(), lv_screen_active(), lv_layer_top(), lv_layer_sys()}) {
        if (root != nullptr) {
            lv_obj_tree_walk(root, &remap_object, &walk);
        }
    }
    lv_obj_report_style_change(nullptr);
}

void init() {
    if (g_ready) {
        return;
    }
    g_ready = true;

    g_key_b19 = ubuntu_b_19;
    g_key_b19.fallback = &lv_font_montserrat_20;
    g_key_b26 = ubuntu_b_26;
    g_key_b26.fallback = &lv_font_montserrat_24;

    reset_box(&g_styles.screen);
    lv_style_set_bg_opa(&g_styles.screen, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.screen, bg());
    lv_style_set_text_color(&g_styles.screen, text());
    lv_style_set_text_font(&g_styles.screen, font_r15());

    reset_box(&g_styles.flat);

    reset_box(&g_styles.card);
    lv_style_set_bg_opa(&g_styles.card, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.card, surface());
    lv_style_set_border_width(&g_styles.card, 1);
    lv_style_set_border_color(&g_styles.card, line());
    lv_style_set_radius(&g_styles.card, kR3);

    // .seg — tło grupy segmentów; same segmenty leżą w nim z odstępem 4 px.
    reset_box(&g_styles.group_box);
    lv_style_set_bg_opa(&g_styles.group_box, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.group_box, surface2());
    lv_style_set_radius(&g_styles.group_box, kR3);
    lv_style_set_pad_all(&g_styles.group_box, kS1);
    lv_style_set_pad_gap(&g_styles.group_box, kS1);

    reset_box(&g_styles.seg_item);
    lv_style_set_radius(&g_styles.seg_item, kR2);
    lv_style_set_text_color(&g_styles.seg_item, dim());
    lv_style_set_text_font(&g_styles.seg_item, font_r13());
    lv_style_set_text_align(&g_styles.seg_item, LV_TEXT_ALIGN_CENTER);

    // Zaznaczenie przez styl i dolną krawędź, nigdy przez zmianę układu.
    reset_box(&g_styles.seg_item_sel);
    lv_style_set_radius(&g_styles.seg_item_sel, kR2);
    lv_style_set_bg_opa(&g_styles.seg_item_sel, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.seg_item_sel, surface3());
    lv_style_set_text_color(&g_styles.seg_item_sel, text());
    lv_style_set_text_font(&g_styles.seg_item_sel, font_m13());
    lv_style_set_border_width(&g_styles.seg_item_sel, 3);
    lv_style_set_border_side(&g_styles.seg_item_sel, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&g_styles.seg_item_sel, accent());

    reset_box(&g_styles.tile);
    lv_style_set_bg_opa(&g_styles.tile, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.tile, surface());
    lv_style_set_border_width(&g_styles.tile, 1);
    lv_style_set_border_color(&g_styles.tile, line());
    lv_style_set_radius(&g_styles.tile, kR3);
    lv_style_set_pad_all(&g_styles.tile, kS2);
    lv_style_set_pad_gap(&g_styles.tile, kS2);
    lv_style_set_text_color(&g_styles.tile, dim());
    lv_style_set_text_font(&g_styles.tile, font_r13());

    reset_box(&g_styles.tile_sel);
    lv_style_set_bg_opa(&g_styles.tile_sel, kAccentSoftOpa);
    lv_style_set_bg_color(&g_styles.tile_sel, accent());
    lv_style_set_border_width(&g_styles.tile_sel, 1);
    lv_style_set_border_color(&g_styles.tile_sel, accent());
    lv_style_set_radius(&g_styles.tile_sel, kR3);
    lv_style_set_text_color(&g_styles.tile_sel, text());

    reset_box(&g_styles.act);
    lv_style_set_bg_opa(&g_styles.act, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.act, surface());
    lv_style_set_border_width(&g_styles.act, 1);
    lv_style_set_border_color(&g_styles.act, line());
    lv_style_set_radius(&g_styles.act, kR3);
    lv_style_set_text_color(&g_styles.act, text());
    lv_style_set_text_font(&g_styles.act, font_r15());
    lv_style_set_pad_gap(&g_styles.act, kS2);

    reset_box(&g_styles.act_primary);
    lv_style_set_bg_opa(&g_styles.act_primary, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.act_primary, accent());
    lv_style_set_border_width(&g_styles.act_primary, 0);
    lv_style_set_radius(&g_styles.act_primary, kR3);
    lv_style_set_text_color(&g_styles.act_primary, accent_ink());
    lv_style_set_text_font(&g_styles.act_primary, font_b15());

    reset_box(&g_styles.list_row);
    lv_style_set_bg_opa(&g_styles.list_row, LV_OPA_COVER);
    lv_style_set_bg_color(&g_styles.list_row, surface());
    lv_style_set_border_width(&g_styles.list_row, 1);
    lv_style_set_border_color(&g_styles.list_row, line());
    lv_style_set_radius(&g_styles.list_row, kR3);
    lv_style_set_pad_hor(&g_styles.list_row, kS4);
    lv_style_set_pad_gap(&g_styles.list_row, kS3);

    // Brak potwierdzenia — obwódka w kolorze błędu. Bez reset_box: styl dokładany
    // na wierzch nie może zerować promienia ani wypełnienia elementu.
    lv_style_init(&g_styles.failed);
    lv_style_set_outline_width(&g_styles.failed, 2);
    lv_style_set_outline_color(&g_styles.failed, danger());
    lv_style_set_outline_opa(&g_styles.failed, LV_OPA_COVER);
    lv_style_set_outline_pad(&g_styles.failed, 0);

    reset_box(&g_styles.cap);
    lv_style_set_text_color(&g_styles.cap, faint());
    lv_style_set_text_font(&g_styles.cap, font_r11());
    lv_style_set_text_letter_space(&g_styles.cap, 2);
}

bool landscape() {
    lv_display_t* display = lv_display_get_default();
    return display != nullptr &&
           lv_display_get_horizontal_resolution(display) > lv_display_get_vertical_resolution(display);
}

void make_flat(lv_obj_t* obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, const_cast<lv_style_t*>(&g_styles.flat), LV_PART_MAIN);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void make_row(lv_obj_t* obj, int32_t gap) {
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(obj, gap, LV_PART_MAIN);
}

void make_column(lv_obj_t* obj, int32_t gap) {
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(obj, gap, LV_PART_MAIN);
}

}  // namespace ui::theme
