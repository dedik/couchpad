#pragma once

// Tokeny systemu Pulpit przeniesione jeden do jednego z docs/ux/mockup-pulpit/pulpit.css.
// Ekrany nie definiują własnych kolorów ani wymiarów — jeśli czegoś brakuje,
// najpierw dochodzi token tutaj.

#include "lvgl.h"

namespace ui::theme {

// ---------- Kolory ----------
// Powierzchnie niosą hierarchię, akcent niesie stan aktywny. Kolor semantyczny
// pojawia się tylko tam, gdzie ma znaczenie: czerwony to wyciszenie i błąd,
// zielony to potwierdzona dostępność.
inline lv_color_t bg() { return lv_color_hex(0x0a0b0e); }
inline lv_color_t surface() { return lv_color_hex(0x14161b); }
inline lv_color_t surface2() { return lv_color_hex(0x1c1f26); }
inline lv_color_t surface3() { return lv_color_hex(0x282c35); }
inline lv_color_t line() { return lv_color_hex(0x30353f); }

inline lv_color_t text() { return lv_color_hex(0xf3f5f9); }
inline lv_color_t dim() { return lv_color_hex(0x98a0ae); }
inline lv_color_t faint() { return lv_color_hex(0x626a78); }

inline lv_color_t accent() { return lv_color_hex(0xf2a93b); }
inline lv_color_t accent_ink() { return lv_color_hex(0x17120a); }
inline lv_color_t ok() { return lv_color_hex(0x56d6a0); }
inline lv_color_t danger() { return lv_color_hex(0xff5f5f); }

// Miękkie tła z makiety to ten sam kolor z krycia 14% i 13%.
inline constexpr lv_opa_t kAccentSoftOpa = 36;
inline constexpr lv_opa_t kDangerSoftOpa = 33;

// ---------- Promienie ----------
inline constexpr int32_t kR1 = 10;
inline constexpr int32_t kR2 = 14;
inline constexpr int32_t kR3 = 18;
inline constexpr int32_t kR4 = 24;
inline constexpr int32_t kRFull = LV_RADIUS_CIRCLE;

// ---------- Rytm odstępów: wielokrotności 4 ----------
inline constexpr int32_t kS1 = 4;
inline constexpr int32_t kS2 = 8;
inline constexpr int32_t kS3 = 12;
inline constexpr int32_t kS4 = 16;
inline constexpr int32_t kS5 = 20;
inline constexpr int32_t kS6 = 24;
inline constexpr int32_t kS7 = 32;

// ---------- Powłoka ----------
// Nagłówek to wiersz tożsamości (nazwa ekranu i zasilanie) z oddechem od górnej
// krawędzi. Pasek statusu (godzina, Wi-Fi) zniknął 2026-11-03 — to pilot, nie telefon;
// łącze widać w ustawieniach. 64 px to dokładna wysokość przycisku zasilania — przy
// mniejszym wierszu LVGL przycina mu zaokrąglenia.
inline constexpr int32_t kIdentityRow = 64;
inline constexpr int32_t kTopbarPad = 5;    // bez niego zasilanie dotykało krawędzi ekranu
inline constexpr int32_t kTopbar = kTopbarPad + kIdentityRow;
// Zakładki z samymi ikonami: 68 px celu dotyku, 8 px nad nim i 4 px pod nim.
inline constexpr int32_t kTabbar = 80;
inline constexpr int32_t kGutter = 24;

// Panel 4,3" ma ok. 8,6 px na milimetr. Najmniejszy dopuszczalny cel to 68 px ≈ 8 mm.
inline constexpr int32_t kTouch = 68;

// Obszar treści po odjęciu nagłówka, paska zakładek i marginesów.
inline constexpr int32_t kContentW = 480 - 2 * kGutter;              // 432
inline constexpr int32_t kContentH = 800 - kTopbar - kTabbar - kS3 - kS4;  // 623

// ---------- Krój ----------
// Ubuntu w odmianach 400, 500 i 700 — tylko te, które krój naprawdę ma.
const lv_font_t* font_r11();
const lv_font_t* font_r13();
const lv_font_t* font_r15();
const lv_font_t* font_m11();
const lv_font_t* font_m13();
const lv_font_t* font_m15();
const lv_font_t* font_b15();
const lv_font_t* font_b19();
const lv_font_t* font_b26();
const lv_font_t* font_b46num();  // cyfry, przecinek, pauza: głośność, kafelek klimy
const lv_font_t* font_b64num();  // to samo w 64 px: tarcza temperatury
// Jak b19/b26, ale z symbolami LVGL (⇧, ⌫) — dla klawiatury ekranowej.
const lv_font_t* font_key_b19();
const lv_font_t* font_key_b26();

// Montserrat jest w projekcie wyłącznie nośnikiem wbudowanych symboli LVGL
// (strzałki, zasilanie, głośność). Tekstu nim nie składamy. Docelowo zastąpi go
// własny zestaw ikon przerobiony tak samo jak ikony zakładek.
const lv_font_t* font_icon();     // 24 px — krzyżak, zasilanie, gładzik
const lv_font_t* font_icon_sm();  // 20 px — wiersze listy, przyciski akcji
const lv_font_t* font_icon_xs();  // 14 px — pasek statusu

// ---------- Style wspólne ----------
// Jeden zestaw na cały program. Ekrany sięgają po nie, nie tworzą własnych.
struct Styles {
    lv_style_t screen;        // tło ekranu
    lv_style_t flat;          // kontener bez tła, bez obramowania, bez marginesów
    lv_style_t card;          // .surface + .line + r3
    lv_style_t group_box;     // .seg: tło surface-2, promień r3, padding 4
    lv_style_t seg_item;      // .seg-item w spoczynku
    lv_style_t seg_item_sel;  // .seg-item.is-sel — tło + bursztynowa dolna krawędź
    lv_style_t tile;          // .tile
    lv_style_t tile_sel;      // .tile.is-sel
    lv_style_t act;           // .act
    lv_style_t act_primary;   // .act.is-primary
    lv_style_t list_row;      // .list-row
    lv_style_t pressed_soft;  // odzew dotyku dla przycisków bez stanu
    lv_style_t pending;       // czeka na snapshot
    lv_style_t failed;        // brak potwierdzenia
    lv_style_t cap;           // etykieta sekcji, wersaliki
};

// Tworzy style raz. Kolejne wywołania nic nie robią.
void init();
const Styles& styles();

// Powtarzalne ustawienia, których nie opłaca się trzymać jako osobny styl.
void make_flat(lv_obj_t* obj);
void make_row(lv_obj_t* obj, int32_t gap);
void make_column(lv_obj_t* obj, int32_t gap);

}  // namespace ui::theme
