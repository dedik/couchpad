#include "ui/theme.hpp"

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
}

namespace ui::theme {
namespace {

Styles g_styles;
bool g_ready = false;

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
const lv_font_t* font_key_b19() { return &g_key_b19; }
const lv_font_t* font_key_b26() { return &g_key_b26; }
const lv_font_t* font_icon() { return &lv_font_montserrat_24; }
const lv_font_t* font_icon_sm() { return &lv_font_montserrat_20; }
const lv_font_t* font_icon_xs() { return &lv_font_montserrat_14; }

const Styles& styles() { return g_styles; }

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

    // Polecenia chwilowe nie mają stanu do potwierdzenia, więc jedyną odpowiedzią
    // na dotknięcie jest ten odzew.
    reset_box(&g_styles.pressed_soft);
    lv_style_set_bg_opa(&g_styles.pressed_soft, kAccentSoftOpa);
    lv_style_set_bg_color(&g_styles.pressed_soft, accent());
    lv_style_set_text_color(&g_styles.pressed_soft, accent());

    // Czeka na snapshot — obwódka w kolorze akcentu.
    reset_box(&g_styles.pending);
    lv_style_set_outline_width(&g_styles.pending, 3);
    lv_style_set_outline_color(&g_styles.pending, accent());
    lv_style_set_outline_opa(&g_styles.pending, kAccentSoftOpa + 60);
    lv_style_set_outline_pad(&g_styles.pending, 0);

    // Brak potwierdzenia — obwódka w kolorze błędu.
    reset_box(&g_styles.failed);
    lv_style_set_outline_width(&g_styles.failed, 2);
    lv_style_set_outline_color(&g_styles.failed, danger());
    lv_style_set_outline_opa(&g_styles.failed, LV_OPA_COVER);
    lv_style_set_outline_pad(&g_styles.failed, 0);

    reset_box(&g_styles.cap);
    lv_style_set_text_color(&g_styles.cap, faint());
    lv_style_set_text_font(&g_styles.cap, font_r11());
    lv_style_set_text_letter_space(&g_styles.cap, 2);
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
