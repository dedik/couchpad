#include "widgets_internal.hpp"

#include <cstring>

#include "core/config.hpp"
#include "storage/settings.hpp"

namespace ui::widgets {

using namespace detail;

// ============================= edytor pola =============================

namespace {

void editor_accept(TextEditor& editor) {
    if (editor.on_accept != nullptr) {
        editor.on_accept(editor.accept_context, lv_textarea_get_text(editor.field));
    }
    close_text_editor(editor);
}

// ---------- klawiatura ekranowa ----------
// Własna macierz zamiast lv_keyboard: gotowa klawiatura LVGL ma 12 wąskich
// klawiszy w rzędzie i własne klawisze sterujące, które na 480 px wyglądały
// jak rząd pigułek. Oba style dzielą zakładki (abc / ąę / 123) i dolny rząd
// (⇧, spacja, ⌫, wyczyść); różnią się układem liter i wyglądem klawiszy.
//
// Pusty klawisz to spacja w mapie: jest ukryty, ale zajmuje miejsce. Dzięki
// temu krótsze rzędy QWERTY są wcięte jak na telefonie, a krótsza zakładka
// nie rozciąga klawiszy na całą wysokość.

struct KeyLayout {
    const char* const* map;
    const uint8_t* widths;  // nullptr: wszystkie po 1
};

// Klawisze specjalne leżą w ostatnim rzędzie macierzy, jak na telefonie:
// ⇧ na początku (tylko przy literach), ⌫ na końcu każdego układu.
constexpr const char* kShift = LV_SYMBOL_UP;
constexpr const char* kDelete = LV_SYMBOL_BACKSPACE;

// Styl Apple TV: siatka 6 kolumn, pięć rzędów.
const char* const kGridLower[] = {
    "a", "b", "c", "d", "e", "f", "\n",
    "g", "h", "i", "j", "k", "l", "\n",
    "m", "n", "o", "p", "q", "r", "\n",
    "s", "t", "u", "v", "w", "x", "\n",
    "y", "z", " ", " ", kShift, kDelete, ""};
const char* const kGridUpper[] = {
    "A", "B", "C", "D", "E", "F", "\n",
    "G", "H", "I", "J", "K", "L", "\n",
    "M", "N", "O", "P", "Q", "R", "\n",
    "S", "T", "U", "V", "W", "X", "\n",
    "Y", "Z", " ", " ", kShift, kDelete, ""};
const char* const kGridPolishLower[] = {
    "ą", "ć", "ę", "ł", "ń", "ó", "\n",
    "ś", "ź", "ż", " ", " ", " ", "\n",
    " ", " ", " ", " ", " ", " ", "\n",
    " ", " ", " ", " ", " ", " ", "\n",
    " ", " ", " ", " ", kShift, kDelete, ""};
const char* const kGridPolishUpper[] = {
    "Ą", "Ć", "Ę", "Ł", "Ń", "Ó", "\n",
    "Ś", "Ź", "Ż", " ", " ", " ", "\n",
    " ", " ", " ", " ", " ", " ", "\n",
    " ", " ", " ", " ", " ", " ", "\n",
    " ", " ", " ", " ", kShift, kDelete, ""};
const char* const kGridDigits[] = {
    "1", "2", "3", "4", "5", "6", "\n",
    "7", "8", "9", "0", "-", "_", "\n",
    "@", ".", ",", ":", "/", "?", "\n",
    "!", "#", "&", "+", "=", "'", "\n",
    "(", ")", "*", "%", "$", kDelete, ""};

// Styl QWERTY: trzy rzędy jak na telefonie. Szerokość 20 jednostek na rząd:
// dziesięć klawiszy po 2, krótsze rzędy wcięte ukrytymi odstępami.
const char* const kQwertyLower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    " ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
    kShift, "z", "x", "c", "v", "b", "n", "m", kDelete, ""};
const char* const kQwertyUpper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    " ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
    kShift, "Z", "X", "C", "V", "B", "N", "M", kDelete, ""};
const uint8_t kQwertyWidths[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1,
    3, 2, 2, 2, 2, 2, 2, 2, 3};
const char* const kQwertyPolishLower[] = {
    " ", "ą", "ć", "ę", "ł", "ń", "ó", "ś", "ź", "ż", " ", "\n",
    " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", "\n",
    kShift, " ", " ", kDelete, ""};
const char* const kQwertyPolishUpper[] = {
    " ", "Ą", "Ć", "Ę", "Ł", "Ń", "Ó", "Ś", "Ź", "Ż", " ", "\n",
    " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", "\n",
    kShift, " ", " ", kDelete, ""};
const uint8_t kQwertyPolishWidths[] = {
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 7, 7, 3};
const char* const kQwertyDigits[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    " ", "-", "/", ":", ";", "(", ")", "$", "&", "@", " ", "\n",
    ".", ",", "?", "!", "'", "#", "%", "+", "=", kDelete, ""};
const uint8_t kQwertyDigitsWidths[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

// Reszta drukowalnego ASCII, której nie ma na zakładce „123” — hasła Wi-Fi
// i brokera mogą zawierać dowolny z tych znaków.
const char* const kGridSymbols[] = {
    "[", "]", "{", "}", "<", ">", "\n",
    "^", "*", "~", "_", "\\", "|", "\n",
    "\"", "`", ";", " ", " ", " ", "\n",
    " ", " ", " ", " ", " ", " ", "\n",
    " ", " ", " ", " ", " ", kDelete, ""};
const char* const kQwertySymbols[] = {
    "[", "]", "{", "}", "<", ">", "^", "*", "~", "_", "\n",
    " ", "\\", "|", "\"", "`", " ", "\n",
    " ", " ", " ", kDelete, ""};
const uint8_t kQwertySymbolsWidths[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    6, 2, 2, 2, 2, 6,
    3, 7, 7, 3};

const char* const kKeyTabs[] = {"abc", "ąę", "123", "#+=", ""};

// Wysokość siatki QWERTY: trzy rzędy klawiszy po ok. 62 px.
constexpr int32_t kQwertyGridH = 3 * 62 + 2 * 8;

using core::config::kKeyboardLiveIntervalMs;

KeyLayout current_layout(const TextEditor& editor) {
    if (editor.qwerty) {
        switch (editor.key_tab) {
            case 1: return {editor.shift ? kQwertyPolishUpper : kQwertyPolishLower,
                            kQwertyPolishWidths};
            case 2: return {kQwertyDigits, kQwertyDigitsWidths};
            case 3: return {kQwertySymbols, kQwertySymbolsWidths};
            default: return {editor.shift ? kQwertyUpper : kQwertyLower, kQwertyWidths};
        }
    }
    switch (editor.key_tab) {
        case 1: return {editor.shift ? kGridPolishUpper : kGridPolishLower, nullptr};
        case 2: return {kGridDigits, nullptr};
        case 3: return {kGridSymbols, nullptr};
        default: return {editor.shift ? kGridUpper : kGridLower, nullptr};
    }
}

void keys_refresh(TextEditor& editor) {
    const KeyLayout layout = current_layout(editor);
    lv_buttonmatrix_set_map(editor.key_grid, const_cast<const char**>(layout.map));
    std::size_t index = 0;
    for (const char* const* key = layout.map; **key != '\0'; ++key) {
        if (std::strcmp(*key, "\n") == 0) {
            continue;
        }
        if (std::strcmp(*key, " ") == 0) {
            lv_buttonmatrix_set_button_ctrl(editor.key_grid, index, LV_BUTTONMATRIX_CTRL_HIDDEN);
        } else {
            lv_buttonmatrix_clear_button_ctrl(editor.key_grid, index,
                                              LV_BUTTONMATRIX_CTRL_HIDDEN);
        }
        lv_buttonmatrix_set_button_width(editor.key_grid, index,
                                         layout.widths != nullptr ? layout.widths[index] : 1);
        // Włączony ⇧ wygląda jak wciśnięty klawisz.
        if (*key == kShift && editor.shift) {
            lv_buttonmatrix_set_button_ctrl(editor.key_grid, index, LV_BUTTONMATRIX_CTRL_CHECKED);
        } else {
            lv_buttonmatrix_clear_button_ctrl(editor.key_grid, index,
                                              LV_BUTTONMATRIX_CTRL_CHECKED);
        }
        ++index;
    }
    lv_buttonmatrix_set_button_ctrl(editor.key_tabs, editor.key_tab,
                                    LV_BUTTONMATRIX_CTRL_CHECKED);
}

void matrix_base_style(lv_obj_t* matrix, const lv_font_t* font) {
    lv_obj_remove_style_all(matrix);
    lv_obj_set_style_pad_all(matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(matrix, theme::kS2, LV_PART_MAIN);
    lv_obj_set_style_pad_column(matrix, theme::kS1, LV_PART_MAIN);
    lv_obj_set_style_text_font(matrix, font, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, theme::text(), LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, theme::kR3, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, LV_PART_ITEMS);
    // Wciśnięty klawisz wygląda jak zaznaczony na Apple TV: jasne pole, ciemny znak.
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(matrix, theme::text(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(matrix, theme::bg(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(matrix, theme::text(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(matrix, theme::bg(), LV_PART_ITEMS | LV_STATE_CHECKED);
}

lv_obj_t* key_action(lv_obj_t* parent, const char* symbol, const char* text) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_height(button, 56);
    lv_obj_set_flex_grow(button, symbol != nullptr ? 1 : 2);
    lv_obj_set_style_radius(button, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_text_color(button, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, theme::text(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, theme::bg(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    if (symbol != nullptr) {
        lv_obj_t* icon = inherited_icon(button, symbol);
        lv_obj_set_style_text_font(icon, theme::font_icon_sm(), LV_PART_MAIN);
    } else {
        lv_obj_t* label = lv_label_create(button);
        lv_obj_remove_style_all(label);
        lv_obj_set_style_text_font(label, theme::font_b15(), LV_PART_MAIN);
        lv_label_set_text(label, text);
    }
    return button;
}

void build_keys(TextEditor& editor) {
    editor.keys = plain(editor.layer);
    lv_obj_set_width(editor.keys, LV_PCT(100));
    lv_obj_set_flex_grow(editor.keys, 1);
    theme::make_column(editor.keys, theme::kS3);
    lv_obj_set_style_pad_bottom(editor.keys, theme::kS4, LV_PART_MAIN);

    editor.key_tabs = lv_buttonmatrix_create(editor.keys);
    matrix_base_style(editor.key_tabs, theme::font_m15());
    lv_obj_set_size(editor.key_tabs, LV_PCT(100), 44);
    lv_obj_set_style_radius(editor.key_tabs, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(editor.key_tabs, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(editor.key_tabs, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(editor.key_tabs, 4, LV_PART_MAIN);
    lv_obj_set_style_text_color(editor.key_tabs, theme::dim(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(editor.key_tabs, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(editor.key_tabs, theme::surface3(),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(editor.key_tabs, theme::text(),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_buttonmatrix_set_map(editor.key_tabs, const_cast<const char**>(kKeyTabs));
    lv_buttonmatrix_set_button_ctrl_all(editor.key_tabs, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(editor.key_tabs, true);
    lv_obj_add_event_cb(
        editor.key_tabs,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            const uint32_t tab = lv_buttonmatrix_get_selected_button(self->key_tabs);
            if (tab < 4) {
                self->key_tab = static_cast<uint8_t>(tab);
                keys_refresh(*self);
            }
        },
        LV_EVENT_VALUE_CHANGED, &editor);

    editor.key_grid = lv_buttonmatrix_create(editor.keys);
    lv_obj_set_width(editor.key_grid, LV_PCT(100));
    lv_obj_add_event_cb(
        editor.key_grid,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            const uint32_t index = lv_buttonmatrix_get_selected_button(self->key_grid);
            if (index == LV_BUTTONMATRIX_BUTTON_NONE) {
                return;
            }
            const char* text = lv_buttonmatrix_get_button_text(self->key_grid, index);
            if (text == nullptr || std::strcmp(text, " ") == 0) {
                return;
            }
            if (std::strcmp(text, kDelete) == 0) {
                lv_textarea_delete_char(self->field);
                return;
            }
            if (std::strcmp(text, kShift) == 0) {
                self->shift = !self->shift;
                keys_refresh(*self);
                return;
            }
            lv_textarea_add_text(self->field, text);
            // Wielka litera tylko dla jednego znaku, jak na telefonie.
            if (self->shift) {
                self->shift = false;
                keys_refresh(*self);
            }
        },
        LV_EVENT_VALUE_CHANGED, &editor);

    lv_obj_t* bottom = plain(editor.keys);
    lv_obj_set_size(bottom, LV_PCT(100), 56);
    theme::make_row(bottom, theme::kS2);

    // ⇧ i ⌫ są w ostatnim rzędzie macierzy, więc spacja może być szeroka jak na telefonie.
    lv_obj_t* space = key_action(bottom, nullptr, "SPACJA");
    lv_obj_set_flex_grow(space, 3);
    lv_obj_add_event_cb(
        space,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            lv_textarea_add_char(self->field, ' ');
        },
        LV_EVENT_CLICKED, &editor);

    lv_obj_t* clear = key_action(bottom, nullptr, "WYCZYŚĆ");
    lv_obj_add_event_cb(
        clear,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            lv_textarea_set_text(self->field, "");
        },
        LV_EVENT_CLICKED, &editor);
}

// Styl Apple TV: duże litery bez tła, siatka zajmuje całe wolne miejsce.
// QWERTY: klawisze z tłem jak na telefonie, trzy rzędy dosunięte do dołu.
void show_keyboard_style(TextEditor& editor) {
    matrix_base_style(editor.key_grid,
                      editor.qwerty ? theme::font_key_b19() : theme::font_key_b26());
    // remove_style_all czyści też rozmiar — bez tego siatka wracała do
    // domyślnej szerokości macierzy i zajmowała pół ekranu.
    lv_obj_set_width(editor.key_grid, LV_PCT(100));
    if (editor.qwerty) {
        lv_obj_set_style_bg_opa(editor.key_grid, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(editor.key_grid, theme::surface2(), LV_PART_ITEMS);
        lv_obj_set_style_radius(editor.key_grid, theme::kR1, LV_PART_ITEMS);
        lv_obj_set_flex_grow(editor.key_grid, 0);
        lv_obj_set_height(editor.key_grid, kQwertyGridH);
        lv_obj_set_flex_align(editor.keys, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
    } else {
        lv_obj_set_flex_grow(editor.key_grid, 1);
        lv_obj_set_flex_align(editor.keys, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
    }
    lv_label_set_text(lv_obj_get_child(editor.style_toggle, 1),
                      editor.qwerty ? "QWERTY" : "Apple TV");
    // Zmiana stylu zmienia wysokość siatki — mapa musi dostać nowy rozmiar.
    // Dwa przebiegi układu: w stylu Apple TV siatka rośnie (flex grow), a LVGL
    // zaznacza wtedy, że jej wysokość ustala układ (`h_layout`). Pierwszy
    // przebieg po przejściu na QWERTY liczy rozmiar jeszcze ze starą flagą
    // i zostawia 408 px zamiast 202 — flagę zdejmuje dopiero w trakcie.
    // Drugi przebieg bierze już wysokość ze stylu (zmierzone na pilocie).
    lv_obj_update_layout(editor.layer);
    lv_obj_mark_layout_as_dirty(editor.key_grid);
    lv_obj_update_layout(editor.layer);
    keys_refresh(editor);
}

void show_live_mode(TextEditor& editor) {
    lv_label_set_text(lv_obj_get_child(editor.live_toggle, 1),
                      editor.live ? "Na żywo" : "Po zapisie");
    lv_obj_set_style_text_color(editor.live_toggle, editor.live ? theme::accent() : theme::dim(),
                                LV_PART_MAIN);
}

void live_send(lv_timer_t* timer) {
    auto* editor = static_cast<TextEditor*>(lv_timer_get_user_data(timer));
    editor->live_timer = nullptr;
    if (editor->on_change != nullptr && editor->live) {
        editor->on_change(editor->accept_context, lv_textarea_get_text(editor->field));
    }
}

// Przycisk-chip w nagłówku edytora: ikona i krótki napis.
lv_obj_t* head_chip(lv_obj_t* parent, const char* symbol) {
    lv_obj_t* chip = lv_button_create(parent);
    lv_obj_remove_style_all(chip);
    lv_obj_set_height(chip, 48);
    lv_obj_set_width(chip, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(chip, theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_radius(chip, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(chip, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_text_color(chip, theme::dim(), LV_PART_MAIN);
    lv_obj_add_style(chip, &styles().pressed_soft, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip, theme::kS2, LV_PART_MAIN);
    lv_obj_t* icon = inherited_icon(chip, symbol);
    lv_obj_set_style_text_font(icon, theme::font_icon_sm(), LV_PART_MAIN);
    lv_obj_t* label = lv_label_create(chip);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, theme::font_m13(), LV_PART_MAIN);
    lv_label_set_text(label, "");
    return chip;
}

}  // namespace

TextEditor* make_text_editor() {
    auto* editor = static_cast<TextEditor*>(lv_malloc_zeroed(sizeof(TextEditor)));

    // Edytor otwiera się z podstron, więc musi leżeć nad nimi.
    editor->layer = plain(lv_layer_sys());
    lv_obj_set_size(editor->layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(editor->layer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(editor->layer, theme::bg(), LV_PART_MAIN);
    lv_obj_add_flag(editor->layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(editor->layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_hor(editor->layer, theme::kGutter, LV_PART_MAIN);
    lv_obj_set_style_pad_top(editor->layer, theme::kS6, LV_PART_MAIN);
    theme::make_column(editor->layer, theme::kS3);

    // Nagłówek: podpis i tytuł po lewej, przełącznik stylu klawiatury po prawej.
    lv_obj_t* head = plain(editor->layer);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_row(head, theme::kS3);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* texts = plain(head);
    lv_obj_set_height(texts, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(texts, 1);
    theme::make_column(texts, theme::kS2);

    editor->caption = make_cap(texts, "");

    // Widoczny tylko dla pól, które umieją przyjmować tekst w trakcie pisania.
    editor->live_toggle = head_chip(head, LV_SYMBOL_REFRESH);
    lv_obj_add_event_cb(
        editor->live_toggle,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            self->live = !self->live;
            storage::set_enabled(storage::Pref::KeyboardLive, self->live);
            show_live_mode(*self);
        },
        LV_EVENT_CLICKED, editor);

    editor->style_toggle = head_chip(head, LV_SYMBOL_KEYBOARD);
    lv_obj_add_event_cb(
        editor->style_toggle,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            self->qwerty = !self->qwerty;
            storage::set_enabled(storage::Pref::KeyboardQwerty, self->qwerty);
            show_keyboard_style(*self);
        },
        LV_EVENT_CLICKED, editor);

    editor->title = lv_label_create(texts);
    lv_obj_remove_style_all(editor->title);
    lv_label_set_long_mode(editor->title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(editor->title, LV_PCT(100));
    lv_obj_set_style_text_font(editor->title, theme::font_b19(), LV_PART_MAIN);
    lv_obj_set_style_text_color(editor->title, theme::text(), LV_PART_MAIN);
    lv_label_set_text(editor->title, "");

    lv_obj_t* field_row = plain(editor->layer);
    lv_obj_set_width(field_row, LV_PCT(100));
    lv_obj_set_height(field_row, 72);
    theme::make_row(field_row, theme::kS2);

    editor->field = lv_textarea_create(field_row);
    lv_obj_remove_style_all(editor->field);
    lv_obj_set_width(editor->field, LV_PCT(100));
    lv_obj_set_height(editor->field, 72);
    lv_obj_set_style_bg_opa(editor->field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(editor->field, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_radius(editor->field, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(editor->field, theme::kS4, LV_PART_MAIN);
    lv_obj_set_style_text_font(editor->field, theme::font_r15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(editor->field, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(editor->field, theme::accent(), LV_PART_CURSOR);
    lv_textarea_set_one_line(editor->field, true);
    lv_obj_set_flex_grow(editor->field, 1);
    lv_obj_add_event_cb(
        editor->field,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            if (self->on_change == nullptr || !self->live || self->live_timer != nullptr) {
                return;
            }
            // Pierwsza zmiana startuje okno, kolejne w nim czekają na jedną wysyłkę.
            self->live_timer = lv_timer_create(live_send, kKeyboardLiveIntervalMs, self);
            lv_timer_set_repeat_count(self->live_timer, 1);
        },
        LV_EVENT_VALUE_CHANGED, editor);

    // Bez podglądu długie hasło wpisuje się w ciemno i jedna literówka kosztuje
    // całą próbę połączenia.
    editor->reveal = lv_button_create(field_row);
    lv_obj_remove_style_all(editor->reveal);
    lv_obj_set_size(editor->reveal, theme::kTouch, 72);
    lv_obj_set_style_radius(editor->reveal, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(editor->reveal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(editor->reveal, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_text_color(editor->reveal, theme::dim(), LV_PART_MAIN);
    lv_obj_add_style(editor->reveal, &styles().pressed_soft, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(editor->reveal, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(editor->reveal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    inherited_icon(editor->reveal, LV_SYMBOL_EYE_OPEN);
    lv_obj_add_event_cb(
        editor->reveal,
        [](lv_event_t* event) {
            auto* self = static_cast<TextEditor*>(lv_event_get_user_data(event));
            const bool hidden = lv_textarea_get_password_mode(self->field);
            lv_textarea_set_password_mode(self->field, !hidden);
            lv_label_set_text(lv_obj_get_child(self->reveal, 0),
                              hidden ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
        },
        LV_EVENT_CLICKED, editor);

    lv_obj_t* duo = make_duo(editor->layer);
    auto* cancel_cb = static_cast<Callback*>(lv_malloc(sizeof(Callback)));
    cancel_cb->context = editor;
    lv_obj_t* cancel = make_action(duo, LV_SYMBOL_CLOSE, "Anuluj", false, ActionLayout::Fill,
                                   nullptr, nullptr);
    lv_obj_add_event_cb(
        cancel,
        [](lv_event_t* event) {
            auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
            close_text_editor(*static_cast<TextEditor*>(data->context));
        },
        LV_EVENT_CLICKED, cancel_cb);
    lv_obj_add_event_cb(
        cancel, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, cancel_cb);

    auto* accept_cb = static_cast<Callback*>(lv_malloc(sizeof(Callback)));
    accept_cb->context = editor;
    lv_obj_t* accept = make_action(duo, LV_SYMBOL_OK, "Zapisz", true, ActionLayout::Fill, nullptr,
                                   nullptr);
    lv_obj_add_event_cb(
        accept,
        [](lv_event_t* event) {
            auto* data = static_cast<Callback*>(lv_event_get_user_data(event));
            editor_accept(*static_cast<TextEditor*>(data->context));
        },
        LV_EVENT_CLICKED, accept_cb);
    lv_obj_add_event_cb(
        accept, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, accept_cb);

    build_keys(*editor);

    lv_obj_add_event_cb(
        editor->layer, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, editor);

    editor->qwerty = storage::enabled(storage::Pref::KeyboardQwerty);
    editor->live = storage::enabled(storage::Pref::KeyboardLive);
    show_keyboard_style(*editor);
    show_live_mode(*editor);
    return editor;
}

void open_text_editor(TextEditor& editor, const char* caption, const char* title,
                      const char* value, EditorMode mode, EditorHandler on_accept,
                      void* context, EditorHandler on_change) {
    editor.on_accept = on_accept;
    editor.accept_context = context;
    editor.on_change = nullptr;   // ustawiany po wpisaniu wartości startowej
    if (on_change != nullptr) {
        lv_obj_remove_flag(editor.live_toggle, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(editor.live_toggle, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(editor.caption, caption);
    lv_label_set_text(editor.title, title);
    editor.secret = mode == EditorMode::Secret;
    lv_textarea_set_password_mode(editor.field, editor.secret);
    lv_label_set_text(lv_obj_get_child(editor.reveal, 0), LV_SYMBOL_EYE_OPEN);
    if (editor.secret) {
        lv_obj_remove_flag(editor.reveal, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(editor.reveal, LV_OBJ_FLAG_HIDDEN);
    }
    lv_textarea_set_text(editor.field, value != nullptr ? value : "");
    editor.on_change = on_change;
    // Port i inne liczby otwierają się od razu na cyfrach.
    editor.key_tab = mode == EditorMode::Number ? 2 : 0;
    editor.shift = false;

    // Styl i tryb mogły się zmienić na stronie „Klawiatura i gładzik”.
    const bool qwerty = storage::enabled(storage::Pref::KeyboardQwerty);
    const bool live = storage::enabled(storage::Pref::KeyboardLive);
    if (qwerty != editor.qwerty) {
        editor.qwerty = qwerty;
        show_keyboard_style(editor);
    }
    if (live != editor.live) {
        editor.live = live;
        show_live_mode(editor);
    }

    lv_obj_remove_flag(editor.layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(editor.layer);
    // Macierz liczy położenie klawiszy z własnego rozmiaru w chwili ustawienia
    // mapy. W ukrytej warstwie LVGL nie liczy układu, więc mapa ustawiona przed
    // pokazaniem dostawała stary rozmiar i klawisze rozciągały się na całą
    // wysokość aż do pierwszego dotknięcia. Najpierw układ, potem mapa.
    lv_obj_update_layout(editor.layer);
    keys_refresh(editor);
}

void close_text_editor(TextEditor& editor) {
    // Najpierw odpinamy wysyłkę na żywo: czyszczenie pola to nie pisanie.
    editor.on_change = nullptr;
    if (editor.live_timer != nullptr) {
        lv_timer_delete(editor.live_timer);
        editor.live_timer = nullptr;
    }
    // Pole czyścimy od razu: nie zostawiamy hasła w pamięci widżetu.
    lv_textarea_set_text(editor.field, "");
    lv_obj_add_flag(editor.layer, LV_OBJ_FLAG_HIDDEN);
    editor.on_accept = nullptr;
    editor.accept_context = nullptr;
}

}  // namespace ui::widgets
