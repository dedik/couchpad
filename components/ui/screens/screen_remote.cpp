#include <cctype>
#include <cstring>

#include "domain/tv_sources.hpp"
#include "domain/z906_options.hpp"
#include "images.h"
#include "ui_internal.hpp"

// Zakładka Pilot (ADR 0020, makieta docs/ux/mockup-remote-panel.html, warianty L3 i P2):
// najczęstsze sterowanie telewizorem, Apple TV i głośnością na jednym ekranie. Polecenia
// te same co na pełnych ekranach — bez nowych tematów MQTT.

namespace ui {
namespace {

using widgets::make_box;
using widgets::make_text;
using widgets::set_visible;

constexpr int32_t kKey = theme::kTouch;     // 68 px ≈ 8 mm — najmniejszy cel dotyku
constexpr int32_t kCardPad = 10;
constexpr int32_t kCardGap = theme::kS2;
constexpr int32_t kBlockGap = 10;
constexpr int32_t kHeaderH = 56;            // przycisk zasilania 48 + poszerzone pole dotyku
constexpr int32_t kSourceH = 52;
constexpr int32_t kStepperH = 52;
constexpr int32_t kChannelH = 52;

// Pion (P2): treść 432 × 692 — TV 210, głośność 90, Apple TV reszta, w nim gładzik 206.
constexpr int32_t kPortraitTrackpad = 206;
// Poziom (L3): pas szuflady 28 + TV 230 | Apple TV | głośność 150; gładzik 280.
constexpr int32_t kEdgeZone = 28;
constexpr int32_t kWideTv = 230;
constexpr int32_t kWideVolume = 150;
constexpr int32_t kWideTrackpad = 280;
constexpr int32_t kSwipe = 36;
constexpr uint32_t kDrawerMs = 180;

const char* const kChannelNames[4] = {"Polsat", "TVN", "TVP1", "TVP2"};
const widgets::TileSpec kChannels[5] = {
    {&logo_polsat, nullptr, nullptr},
    {&logo_tvn, nullptr, nullptr},
    {&logo_tvp1, nullptr, nullptr},
    {&logo_tvp2, nullptr, nullptr},
    {&icon_fx_more, nullptr, nullptr},
};

Context& ctx(void* raw) { return *static_cast<Context*>(raw); }

// ---------- klocki ----------

lv_obj_t* make_card(lv_obj_t* parent) {
    lv_obj_t* card = make_box(parent);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);  // dotyk obok klawiszy nie przecieka niżej
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_radius(card, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, kCardPad, LV_PART_MAIN);
    theme::make_column(card, kCardGap);
    return card;
}

lv_obj_t* make_row(lv_obj_t* parent, int32_t height, int32_t gap = kCardGap) {
    lv_obj_t* row = make_box(parent);
    lv_obj_set_size(row, LV_PCT(100), height);
    theme::make_row(row, gap);
    return row;
}

// Klawisz: obrys, wciśnięty — tło surface-2. Treść ustawia wołający.
lv_obj_t* make_key_base(lv_obj_t* parent) {
    lv_obj_t* key = widgets::make_bare_button(parent);
    lv_obj_set_style_radius(key, theme::kR2, LV_PART_MAIN);
    lv_obj_set_style_border_width(key, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(key, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(key, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(key, theme::surface2(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(key, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(key, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(key, theme::kS1, LV_PART_MAIN);
    return key;
}

// Ikona pixel-art nad podpisem.
lv_obj_t* make_icon_key(lv_obj_t* parent, const lv_image_dsc_t* icon, const char* text) {
    lv_obj_t* key = make_key_base(parent);
    lv_obj_t* image = lv_image_create(key);
    lv_image_set_src(image, icon);
    lv_obj_t* label = make_text(key, theme::font_m13(), theme::dim());
    lv_label_set_text(label, text);
    return key;
}

// Krok (P±, głośność ±): „P” i znak albo sam znak. Przy `repeat` przytrzymanie powtarza krok.
lv_obj_t* make_step_key(lv_obj_t* parent, const char* prefix, const char* symbol, bool repeat,
                        void (*handler)(void*), void* context) {
    lv_obj_t* key = make_key_base(parent);
    lv_obj_set_flex_flow(key, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(key, theme::kS2, LV_PART_MAIN);
    if (prefix != nullptr) {
        lv_obj_t* label = make_text(key, theme::font_b26(), theme::text());
        lv_label_set_text(label, prefix);
    }
    lv_obj_t* sign = make_text(key, theme::font_icon(), theme::text());
    lv_label_set_text(sign, symbol);
    auto press = [](lv_event_t* event) {
        auto* fn = reinterpret_cast<void (*)(void*)>(lv_event_get_user_data(event));
        fn(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_current_target(event))));
    };
    lv_obj_set_user_data(key, context);
    lv_obj_add_event_cb(key, press, repeat ? LV_EVENT_PRESSED : LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(handler));
    if (repeat) {
        lv_obj_add_event_cb(key, press, LV_EVENT_LONG_PRESSED_REPEAT, reinterpret_cast<void*>(handler));
    }
    return key;
}

void on_click(lv_obj_t* obj, void (*handler)(void*), void* context) {
    lv_obj_set_user_data(obj, context);
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t* event) {
            auto* fn = reinterpret_cast<void (*)(void*)>(lv_event_get_user_data(event));
            fn(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_current_target(event))));
        },
        LV_EVENT_CLICKED, reinterpret_cast<void*>(handler));
}

// Nagłówek bloku: ikona, nazwa i stan; zwraca etykietę stanu. `trailing` — wiersz, do którego
// wołający dokłada np. zasilanie.
lv_obj_t* make_block_header(lv_obj_t* card, const lv_image_dsc_t* icon, const char* title,
                            lv_obj_t** row_out = nullptr) {
    lv_obj_t* row = make_row(card, kHeaderH, theme::kS3);
    lv_obj_t* image = lv_image_create(row);
    lv_image_set_src(image, icon);
    lv_obj_t* texts = make_box(row);
    lv_obj_set_height(texts, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(texts, 1);
    theme::make_column(texts, 2);
    lv_obj_t* name = make_text(texts, theme::font_m15(), theme::text());
    lv_label_set_text(name, title);
    lv_obj_t* status = make_text(texts, theme::font_r13(), theme::dim());
    lv_obj_set_width(status, LV_PCT(100));
    lv_label_set_long_mode(status, LV_LABEL_LONG_DOT);
    if (row_out != nullptr) {
        *row_out = row;
    }
    return status;
}

// ---------- Telewizor ----------

void on_source(void* raw) {
    auto& source = *static_cast<RemoteScreen::Source*>(raw);
    Context& context = *source.context;
    const domain::AppState& state = context.controller->state();
    const domain::TvState& tv = state.snapshot().tv;
    if (!state.tv_fresh() || source.option >= tv.source_options.size() ||
        tv.source_options[source.option] == tv.source) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::TvSource);
    intent.text = tv.source_options[source.option];
    intent.payload.assign(source.option < tv.source_ids.size() ? tv.source_ids[source.option].c_str() : "");
    dispatch(context, intent);
}

// Tryb obrazu o `delta` dalej, w kółko. Liczy od wyboru w toku, żeby szybkie stuknięcia się sumowały.
void step_picture(Context& context, int delta) {
    const domain::AppState& state = context.controller->state();
    const domain::TvState& tv = state.snapshot().tv;
    const std::size_t count = tv.picture_mode_options.size();
    if (!state.tv_fresh() || count == 0) {
        return;
    }
    const domain::Label* current = &tv.picture_mode;
    if (const domain::PendingEntry* pending = state.pending_of(domain::ControlId::TvPicture)) {
        if (!pending->requested.empty()) {
            current = &pending->requested;
        }
    }
    std::size_t index = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (tv.picture_mode_options[i] == *current) {
            index = i;
        }
    }
    const std::size_t next = (index + count + delta) % count;
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::TvPicture);
    intent.text = tv.picture_mode_options[next];
    intent.payload.assign(next < tv.picture_mode_ids.size() ? tv.picture_mode_ids[next].c_str() : "");
    dispatch(context, intent);
}

void on_picture_prev(void* raw) { step_picture(ctx(raw), -1); }
void on_picture_next(void* raw) { step_picture(ctx(raw), 1); }
void on_picture_list(void* raw) { tv_open_picture_picker(ctx(raw)); }

void on_tv_back(void* raw) {
    if (ctx(raw).controller->state().tv_fresh()) {
        tv_send_key(ctx(raw), domain::NavKey::Back);
    }
}
void on_channel_up(void* raw) {
    if (ctx(raw).controller->state().tv_fresh()) {
        tv_step_channel(ctx(raw), true);
    }
}
void on_channel_down(void* raw) {
    if (ctx(raw).controller->state().tv_fresh()) {
        tv_step_channel(ctx(raw), false);
    }
}

void on_channel_tile(void* raw, std::size_t index) {
    Context& context = ctx(raw);
    if (!context.controller->state().tv_fresh()) {
        return;
    }
    if (index < 4) {
        tv_channel_shortcut(context, index);
    } else {
        tv_open_channel_menu(context);
    }
}

void build_sources(Context& context, lv_obj_t* card, bool stacked) {
    RemoteScreen& screen = context.remote;
    lv_obj_t* row = make_row(card, kSourceH, 6);
    for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
        RemoteScreen::Source& source = screen.sources[i];
        source.context = &context;
        source.root = make_key_base(row);
        lv_obj_set_height(source.root, LV_PCT(100));
        lv_obj_set_flex_grow(source.root, 1);
        lv_obj_set_style_pad_hor(source.root, theme::kS1, LV_PART_MAIN);
        if (!stacked) {
            lv_obj_set_flex_flow(source.root, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_pad_column(source.root, 6, LV_PART_MAIN);
        } else {
            lv_obj_set_style_pad_row(source.root, 2, LV_PART_MAIN);
        }
        source.icon = lv_image_create(source.root);
        source.label = make_text(source.root, theme::font_m13(), theme::dim());
        lv_label_set_long_mode(source.label, LV_LABEL_LONG_DOT);
        on_click(source.root, &on_source, &source);
        lv_obj_add_flag(source.root, LV_OBJ_FLAG_HIDDEN);
    }
}

void build_picture(Context& context, lv_obj_t* card) {
    RemoteScreen& screen = context.remote;
    screen.picture = make_row(card, kStepperH, 0);
    lv_obj_set_style_border_width(screen.picture, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(screen.picture, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_radius(screen.picture, theme::kR2, LV_PART_MAIN);

    auto arrow = [&](const char* symbol, void (*handler)(void*)) {
        lv_obj_t* key = widgets::make_bare_button(screen.picture);
        lv_obj_set_size(key, 56, LV_PCT(100));
        lv_obj_set_flex_flow(key, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(key, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_radius(key, theme::kR2, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(key, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(key, theme::surface2(), LV_STATE_PRESSED);
        lv_obj_t* sign = make_text(key, theme::font_icon(), theme::dim());
        lv_label_set_text(sign, symbol);
        on_click(key, handler, &context);
        return key;
    };
    arrow(LV_SYMBOL_LEFT, &on_picture_prev);

    // Nazwa trybu otwiera pełną listę (ta sama co na ekranie TV).
    lv_obj_t* middle = widgets::make_bare_button(screen.picture);
    lv_obj_set_height(middle, LV_PCT(100));
    lv_obj_set_flex_grow(middle, 1);
    theme::make_column(middle, 5);
    lv_obj_set_flex_align(middle, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    screen.picture_label = make_text(middle, theme::font_b15(), theme::text());
    lv_label_set_long_mode(screen.picture_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(screen.picture_label, LV_PCT(100));
    lv_obj_set_style_text_align(screen.picture_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t* dots = make_box(middle);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, 5);
    theme::make_row(dots, 4);
    for (lv_obj_t*& dot : screen.picture_dots) {
        dot = make_box(dots);
        lv_obj_set_size(dot, 5, 5);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, theme::surface3(), LV_PART_MAIN);
    }
    on_click(middle, &on_picture_list, &context);

    arrow(LV_SYMBOL_RIGHT, &on_picture_next);
}

void build_tv_keys(Context& context, lv_obj_t* card) {
    lv_obj_t* row = make_row(card, kKey);
    lv_obj_t* back = make_icon_key(row, &icon_key_back, "Cofnij");
    on_click(back, &on_tv_back, &context);
    for (lv_obj_t* key : {back,
                          make_step_key(row, "P", LV_SYMBOL_MINUS, false, &on_channel_down, &context),
                          make_step_key(row, "P", LV_SYMBOL_PLUS, false, &on_channel_up, &context)}) {
        lv_obj_set_height(key, LV_PCT(100));
        lv_obj_set_flex_grow(key, 1);
    }
}

// ---------- Apple TV ----------

void on_atv_key(void* raw, domain::NavKey key) {
    if (ctx(raw).controller->state().appletv_fresh()) {
        appletv_send_key(ctx(raw), key, domain::Press::Tap);
    }
}
void on_atv_hold(void* raw, domain::NavKey key) {
    if (ctx(raw).controller->state().appletv_fresh()) {
        appletv_send_key(ctx(raw), key, domain::Press::Hold);
    }
}
void on_atv_menu(void* raw) { on_atv_key(raw, domain::NavKey::Menu); }
void on_atv_home(void* raw, domain::Press press) {
    if (ctx(raw).controller->state().appletv_fresh()) {
        appletv_send_key(ctx(raw), domain::NavKey::Home, press);
    }
}
void on_atv_keyboard(void* raw) {
    if (ctx(raw).controller->state().appletv_fresh()) {
        appletv_open_keyboard(ctx(raw));
    }
}
void on_atv_power(void* raw) {
    Context& context = ctx(raw);
    if (!context.controller->state().appletv_fresh()) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetPower, domain::ControlId::AtvPower);
    intent.flag = context.controller->state().snapshot().appletv.power != domain::Power::On;
    dispatch(context, intent);
}

void build_atv(Context& context, lv_obj_t* card, int32_t trackpad) {
    RemoteScreen& screen = context.remote;
    lv_obj_t* header = nullptr;
    screen.atv_status = make_block_header(card, &icon_appletv, "Apple TV", &header);
    screen.atv_power = widgets::make_power_button(header, &on_atv_power, &context);

    lv_obj_t* fill = widgets::make_fill(card);
    widgets::make_trackpad(fill, &on_atv_key, &context, &on_atv_hold, trackpad);

    lv_obj_t* row = make_row(card, kKey);
    lv_obj_t* menu = make_icon_key(row, &icon_key_menu, "Menu");
    on_click(menu, &on_atv_menu, &context);
    // Home jak na pilocie Siri: stuknięcie, ×2 i przytrzymanie.
    lv_obj_t* home = make_icon_key(row, &icon_key_home, "Home");
    widgets::attach_presses(home, &on_atv_home, &context);
    lv_obj_t* keyboard = make_icon_key(row, &icon_key_keyboard, "Klawiatura");
    on_click(keyboard, &on_atv_keyboard, &context);
    for (lv_obj_t* key : {menu, home, keyboard}) {
        lv_obj_set_height(key, LV_PCT(100));
        lv_obj_set_flex_grow(key, 1);
    }
}

// ---------- głośność ----------

void on_volume_up(void* raw) { z906_step_volume(ctx(raw), 1); }
void on_volume_down(void* raw) { z906_step_volume(ctx(raw), -1); }
void on_mute(void* raw) {
    Context& context = ctx(raw);
    if (!context.controller->state().z906_fresh()) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetMuted, domain::ControlId::ZMute);
    intent.flag = !context.controller->state().snapshot().z906.muted;
    dispatch(context, intent);
}

lv_obj_t* make_mute(Context& context, lv_obj_t* parent) {
    RemoteScreen& screen = context.remote;
    screen.mute = make_key_base(parent);
    screen.mute_icon = lv_image_create(screen.mute);
    lv_image_set_src(screen.mute_icon, &icon_key_volume);
    on_click(screen.mute, &on_mute, &context);
    return screen.mute;
}

lv_obj_t* make_volume_value(lv_obj_t* parent) {
    lv_obj_t* value = make_text(parent, theme::font_b46num(), theme::text());
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(value, "—");
    return value;
}

// Pion: [kolumna] − wartość + [wyciszenie] w jednym rzędzie 68 px.
void build_volume_row(Context& context, lv_obj_t* card) {
    RemoteScreen& screen = context.remote;
    lv_obj_t* row = make_row(card, kKey);
    lv_obj_t* badge = make_box(row);
    lv_obj_set_size(badge, 64, LV_PCT(100));
    theme::make_column(badge, 0);
    lv_obj_set_flex_align(badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* icon = lv_image_create(badge);
    lv_image_set_src(icon, &icon_audio);
    screen.audio_status = make_text(badge, theme::font_r11(), theme::dim());
    lv_label_set_long_mode(screen.audio_status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(screen.audio_status, LV_PCT(100));
    lv_obj_set_style_text_align(screen.audio_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t* minus = make_step_key(row, nullptr, LV_SYMBOL_MINUS, true, &on_volume_down, &context);
    lv_obj_set_size(minus, 84, LV_PCT(100));
    screen.volume = make_volume_value(row);
    lv_obj_set_flex_grow(screen.volume, 1);
    lv_obj_t* plus = make_step_key(row, nullptr, LV_SYMBOL_PLUS, true, &on_volume_up, &context);
    lv_obj_set_size(plus, 84, LV_PCT(100));
    lv_obj_set_size(make_mute(context, row), kKey, LV_PCT(100));
}

// Poziom: kolumna — ikona i wejście, +, wartość, −, wyciszenie.
// Efekty Z906 jawnie (makieta docs/ux/mockup-remote-volume.html, G2): krótkie podpisy,
// polecenie idzie pełną nazwą z domain::z906_effects() („Stereo 3D”, „No effect”).
const char* const kEffectShort[4] = {"3D", "2.1", "4.1", "Brak"};

void on_effect(void* raw) {
    auto& effect = *static_cast<RemoteScreen::Effect*>(raw);
    Context& context = *effect.context;
    const domain::AppState& state = context.controller->state();
    const domain::OptionList& effects = domain::z906_effects();
    if (!state.z906_fresh() || effect.index >= effects.size() ||
        effects[effect.index] == state.snapshot().z906.effect) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetOption, domain::ControlId::ZEffect);
    intent.text = effects[effect.index];
    dispatch(context, intent);
}

void on_audio_input(void* raw) { z906_open_source_picker(ctx(raw)); }

// Poziom: kolumna 150 px — nagłówek (ikona, Z906, Decode 5.1), wejście (lista), efekty 2 × 2,
// wartość, −/+ w jednym rzędzie, wyciszenie.
void build_volume_column(Context& context, lv_obj_t* card) {
    RemoteScreen& screen = context.remote;
    // Nagłówek bloku jak przy Telewizorze i Apple TV: ikona, nazwa, stan (Decode 5.1).
    screen.audio_status = make_block_header(card, &icon_audio, "Z906");

    // Wejście: bieżąca wartość bez podpisu (mówi sama za siebie), stuknięcie otwiera listę
    // z ekranu Głośniki.
    screen.audio_input = make_key_base(card);
    lv_obj_set_size(screen.audio_input, LV_PCT(100), kSourceH);
    lv_obj_set_flex_align(screen.audio_input, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(screen.audio_input, theme::kS3, LV_PART_MAIN);
    lv_obj_t* value_row = make_box(screen.audio_input);
    lv_obj_set_size(value_row, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_row(value_row, 4);
    screen.audio_input_value = make_text(value_row, theme::font_m15(), theme::text());
    lv_label_set_long_mode(screen.audio_input_value, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(screen.audio_input_value, 1);
    lv_obj_t* chevron = make_text(value_row, theme::font_icon_xs(), theme::dim());  // symbol LVGL, nie znak Ubuntu
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    on_click(screen.audio_input, &on_audio_input, &context);

    lv_obj_t* grid = make_box(card);
    lv_obj_set_size(grid, LV_PCT(100), 2 * 40 + 6);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(grid, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, 6, LV_PART_MAIN);
    // Kolejność na ekranie: 3D, 2.1, 4.1, Brak — tak jak w domain::z906_effects().
    for (std::size_t i = 0; i < 4; ++i) {
        RemoteScreen::Effect& effect = screen.effects[i];
        effect.context = &context;
        effect.index = i;
        effect.root = make_key_base(grid);
        lv_obj_set_size(effect.root, (150 - 2 * kCardPad - 2 - 6) / 2, 40);
        effect.label = make_text(effect.root, theme::font_m13(), theme::dim());
        lv_label_set_text(effect.label, kEffectShort[i]);
        on_click(effect.root, &on_effect, &effect);
    }

    widgets::make_spacer(card);
    screen.volume = make_volume_value(card);
    lv_obj_set_width(screen.volume, LV_PCT(100));
    lv_obj_t* steps = make_row(card, 56, 6);
    for (lv_obj_t* key : {make_step_key(steps, nullptr, LV_SYMBOL_MINUS, true, &on_volume_down, &context),
                          make_step_key(steps, nullptr, LV_SYMBOL_PLUS, true, &on_volume_up, &context)}) {
        lv_obj_set_height(key, LV_PCT(100));
        lv_obj_set_flex_grow(key, 1);
    }
    lv_obj_set_size(make_mute(context, card), LV_PCT(100), 48);
}

void render_effects(Context& context) {
    RemoteScreen& screen = context.remote;
    if (screen.audio_input == nullptr) {
        return;
    }
    const domain::AppState& state = context.controller->state();
    const domain::Z906State& z = state.snapshot().z906;
    const bool fresh = state.z906_fresh();

    const domain::PendingEntry* source = state.pending_of(domain::ControlId::ZSource);
    const char* input = source != nullptr && !source->requested.empty() ? source->requested.c_str()
                        : fresh ? z.source.c_str()
                                : "—";
    lv_label_set_text(screen.audio_input_value, input);
    lv_obj_set_style_border_color(screen.audio_input, source != nullptr ? theme::accent() : theme::line(),
                                  LV_PART_MAIN);
    widgets::set_status_outline(screen.audio_input, state.status_of(domain::ControlId::ZSource));

    const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ZEffect);
    const domain::OptionList& effects = domain::z906_effects();
    for (std::size_t i = 0; i < 4 && i < effects.size(); ++i) {
        const RemoteScreen::Effect& effect = screen.effects[i];
        const bool active = fresh && effects[i] == z.effect;
        const bool requested = pending != nullptr && effects[i] == pending->requested;
        lv_obj_set_style_bg_opa(effect.root, active ? theme::kAccentSoftOpa : theme::kOpaTransp, LV_PART_MAIN);
        lv_obj_set_style_bg_color(effect.root, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_border_color(effect.root, active || requested ? theme::accent() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(effect.root, requested && !active ? 140 : LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(effect.label, active ? theme::text() : theme::dim(), LV_PART_MAIN);
        widgets::set_status_outline(effect.root, requested ? state.status_of(domain::ControlId::ZEffect)
                                                           : domain::ControlStatus::Idle);
    }
}

// ---------- szuflada z zakładkami (poziom) ----------

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

void set_drawer(Context& context, bool open, bool animate) {
    RemoteScreen& screen = context.remote;
    if (screen.drawer == nullptr) {
        return;
    }
    screen.drawer_open = open;
    screen.swiping = false;
    slide(screen.drawer, open ? 0 : -theme::kRail - 12, animate);
    set_visible(screen.scrim, open);
}

void on_edge(lv_event_t* event) {
    Context& context = ctx(lv_event_get_user_data(event));
    RemoteScreen& screen = context.remote;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            screen.swiping = true;
            screen.swipe_x = touch_x();
            break;
        case LV_EVENT_PRESSING:
            if (screen.swiping && touch_x() - screen.swipe_x >= kSwipe) {
                set_drawer(context, true, true);
            }
            break;
        case LV_EVENT_CLICKED:
            if (!screen.drawer_open) {
                set_drawer(context, true, true);
            }
            break;
        default:
            break;
    }
}

void on_scrim(lv_event_t* event) {
    Context& context = ctx(lv_event_get_user_data(event));
    RemoteScreen& screen = context.remote;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            screen.swipe_x = touch_x();
            break;
        case LV_EVENT_PRESSING:
            if (screen.drawer_open && screen.swipe_x - touch_x() >= kSwipe) {
                set_drawer(context, false, true);
            }
            break;
        case LV_EVENT_CLICKED:
            set_drawer(context, false, true);
            break;
        default:
            break;
    }
}

void on_drawer_tab(lv_event_t* event) {
    Context& context = ctx(lv_event_get_user_data(event));
    auto* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    const auto index = static_cast<std::size_t>(lv_obj_get_index(target));
    set_drawer(context, false, false);
    if (index < kScreenCount) {
        show_screen(context, static_cast<ScreenId>(index));
    }
}

void build_drawer(Context& context) {
    RemoteScreen& screen = context.remote;
    lv_obj_t* root = context.shell.root;

    screen.edge = widgets::make_bare_button(root);
    lv_obj_add_flag(screen.edge, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(screen.edge, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_size(screen.edge, kEdgeZone, LV_PCT(100));
    lv_obj_set_pos(screen.edge, 0, 0);
    lv_obj_t* handle = make_box(screen.edge);
    lv_obj_set_size(handle, 5, 64);
    lv_obj_set_style_radius(handle, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(handle, theme::surface3(), LV_PART_MAIN);
    lv_obj_align(handle, LV_ALIGN_LEFT_MID, 5, 0);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_CLICKED}) {
        lv_obj_add_event_cb(screen.edge, on_edge, code, &context);
    }

    screen.scrim = widgets::make_bare_button(root);
    lv_obj_add_flag(screen.scrim, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(screen.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(screen.scrim, 0, 0);
    lv_obj_set_style_bg_opa(screen.scrim, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.scrim, lv_color_black(), LV_PART_MAIN);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_CLICKED}) {
        lv_obj_add_event_cb(screen.scrim, on_scrim, code, &context);
    }

    screen.drawer = make_box(root);
    lv_obj_add_flag(screen.drawer, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.drawer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(screen.drawer, theme::kRail, LV_PCT(100));
    lv_obj_set_y(screen.drawer, 0);
    lv_obj_set_style_bg_opa(screen.drawer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.drawer, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_side(screen.drawer, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen.drawer, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(screen.drawer, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.drawer, theme::kS2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(screen.drawer, 30, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(screen.drawer, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(screen.drawer, lv_color_black(), LV_PART_MAIN);
    theme::make_column(screen.drawer, theme::kS2);
    lv_obj_set_flex_align(screen.drawer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (std::size_t i = 0; i < kScreenCount; ++i) {
        lv_obj_t* tab = widgets::make_bare_button(screen.drawer);
        lv_obj_set_size(tab, LV_PCT(100), theme::kRailTab);
        lv_obj_set_style_radius(tab, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tab, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tab, i == static_cast<std::size_t>(ScreenId::Remote) ? theme::kAccentSoftOpa
                                                                                    : theme::kOpaTransp,
                                LV_PART_MAIN);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* icon = lv_image_create(tab);
        lv_image_set_src(icon, tab_icon(static_cast<ScreenId>(i)));
        lv_obj_add_event_cb(tab, on_drawer_tab, LV_EVENT_CLICKED, &context);
    }
    set_drawer(context, false, false);
}

// ---------- rysowanie ----------

const lv_image_dsc_t* source_icon(const domain::TvState& tv, std::size_t option) {
    if (domain::is_live_tv(tv, option)) {
        return &icon_tv_small;
    }
    const char* label = tv.source_options[option].c_str();
    for (const char* p = label; *p != '\0'; ++p) {
        if (std::tolower(static_cast<unsigned char>(p[0])) == 'a' && std::strncmp(p + 1, "pple", 4) == 0) {
            return &icon_appletv_small;
        }
    }
    return &icon_key_home;
}

void render_sources(Context& context) {
    RemoteScreen& screen = context.remote;
    const domain::AppState& state = context.controller->state();
    const domain::TvState& tv = state.snapshot().tv;
    const domain::RemoteSources list = domain::remote_sources(tv);
    const domain::PendingEntry* pending = state.pending_of(domain::ControlId::TvSource);
    for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
        RemoteScreen::Source& source = screen.sources[i];
        if (i >= list.count) {
            lv_obj_add_flag(source.root, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(source.root, LV_OBJ_FLAG_HIDDEN);
        source.option = list.index[i];
        lv_image_set_src(source.icon, source_icon(tv, source.option));
        // Telewizja skrótem, żeby trzy wejścia zmieściły się w rzędzie.
        lv_label_set_text(source.label, domain::is_live_tv(tv, source.option)
                                            ? "TV"
                                            : tv.source_options[source.option].c_str());
        const bool active = tv.source_options[source.option] == tv.source;
        const bool requested = pending != nullptr && tv.source_options[source.option] == pending->requested;
        lv_obj_set_style_bg_opa(source.root, active ? theme::kAccentSoftOpa : theme::kOpaTransp, LV_PART_MAIN);
        lv_obj_set_style_bg_color(source.root, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_border_color(source.root, active || requested ? theme::accent() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(source.root, requested && !active ? 140 : LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(source.label, active ? theme::text() : theme::dim(), LV_PART_MAIN);
        widgets::set_status_outline(source.root, requested ? state.status_of(domain::ControlId::TvSource)
                                                           : domain::ControlStatus::Idle);
    }
}

void render_picture(Context& context) {
    RemoteScreen& screen = context.remote;
    const domain::AppState& state = context.controller->state();
    const domain::TvState& tv = state.snapshot().tv;
    const domain::PendingEntry* pending = state.pending_of(domain::ControlId::TvPicture);
    const domain::Label* shown = &tv.picture_mode;
    if (pending != nullptr && !pending->requested.empty()) {
        shown = &pending->requested;
    }
    lv_label_set_text(screen.picture_label, shown->empty() ? "—" : shown->c_str());
    const std::size_t count = tv.picture_mode_options.size();
    for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
        set_visible(screen.picture_dots[i], count > 1 && i < count);
        const bool current = i < count && tv.picture_mode_options[i] == *shown;
        lv_obj_set_style_bg_color(screen.picture_dots[i], current ? theme::accent() : theme::surface3(),
                                  LV_PART_MAIN);
    }
    lv_obj_set_style_border_color(screen.picture, pending != nullptr ? theme::accent() : theme::line(),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_opa(screen.picture, pending != nullptr ? 140 : LV_OPA_COVER, LV_PART_MAIN);
    widgets::set_status_outline(screen.picture, state.status_of(domain::ControlId::TvPicture));
}

void dim_block(lv_obj_t* card, bool fresh) {
    lv_obj_set_style_opa(card, fresh ? LV_OPA_COVER : 90, LV_PART_MAIN);
}

}  // namespace

void build_remote(Context& context, lv_obj_t* parent) {
    RemoteScreen& screen = context.remote;
    screen.section = make_box(parent);
    lv_obj_set_size(screen.section, LV_PCT(100), LV_PCT(100));
    const bool wide = theme::landscape();

    if (wide) {
        theme::make_row(screen.section, kBlockGap);
        lv_obj_set_flex_align(screen.section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        screen.tv_card = make_card(screen.section);
        lv_obj_set_size(screen.tv_card, kWideTv, LV_PCT(100));
        screen.tv_status = make_block_header(screen.tv_card, &icon_tv, "Telewizor");
        build_sources(context, screen.tv_card, true);
        build_picture(context, screen.tv_card);
        widgets::make_tiles(screen.tv_card, kChannels, 5, 3, kChannelH, &on_channel_tile, &context);
        widgets::make_spacer(screen.tv_card);
        build_tv_keys(context, screen.tv_card);

        screen.atv_card = make_card(screen.section);
        lv_obj_set_height(screen.atv_card, LV_PCT(100));
        lv_obj_set_flex_grow(screen.atv_card, 1);
        build_atv(context, screen.atv_card, kWideTrackpad);

        screen.volume_card = make_card(screen.section);
        lv_obj_set_size(screen.volume_card, kWideVolume, LV_PCT(100));
        build_volume_column(context, screen.volume_card);

        build_drawer(context);
        return;
    }

    theme::make_column(screen.section, kBlockGap);
    screen.tv_card = make_card(screen.section);
    lv_obj_set_size(screen.tv_card, LV_PCT(100), LV_SIZE_CONTENT);
    build_sources(context, screen.tv_card, false);
    build_picture(context, screen.tv_card);
    build_tv_keys(context, screen.tv_card);

    screen.atv_card = make_card(screen.section);
    lv_obj_set_width(screen.atv_card, LV_PCT(100));
    lv_obj_set_flex_grow(screen.atv_card, 1);
    build_atv(context, screen.atv_card, kPortraitTrackpad);

    screen.volume_card = make_card(screen.section);
    lv_obj_set_size(screen.volume_card, LV_PCT(100), LV_SIZE_CONTENT);
    build_volume_row(context, screen.volume_card);
}

void render_remote(Context& context) {
    RemoteScreen& screen = context.remote;
    if (screen.section == nullptr) {
        return;
    }
    const domain::AppState& state = context.controller->state();
    const domain::DeviceSnapshot& snapshot = state.snapshot();

    // Szuflada należy do Pilota: poza nim schowana razem z pasem przy krawędzi.
    if (screen.edge != nullptr) {
        const bool here = context.screen == ScreenId::Remote && !context.start.open;
        set_visible(screen.edge, here);
        if (!here && screen.drawer_open) {
            set_drawer(context, false, false);
        }
    }

    // Telewizor.
    const bool tv_fresh = state.tv_fresh();
    render_sources(context);
    render_picture(context);
    if (screen.tv_status != nullptr) {
        char text[64];
        const domain::TvState& tv = snapshot.tv;
        if (!tv_fresh) {
            lv_snprintf(text, sizeof(text), "brak stanu");
        } else if (tv.power != domain::Power::On) {
            lv_snprintf(text, sizeof(text), "wyłączony");
        } else {
            // Wejście i tryb obrazu widać w kafelkach pod nagłówkiem — tu tylko stan.
            lv_snprintf(text, sizeof(text), "włączony");
        }
        lv_label_set_text(screen.tv_status, text);
    }
    dim_block(screen.tv_card, tv_fresh);

    // Apple TV.
    const bool atv_fresh = state.appletv_fresh();
    const domain::AppleTvState& atv = snapshot.appletv;
    const char* app = "";
    for (std::size_t i = 0; i < atv.app_count; ++i) {
        if (!atv.app_id.empty() && atv.apps[i].id == atv.app_id) {
            app = atv.apps[i].label.c_str();
        }
    }
    const bool atv_on = atv.power == domain::Power::On;
    lv_label_set_text(screen.atv_status, !atv_fresh ? "brak stanu"
                                         : !atv_on  ? "wyłączony"
                                         : *app != '\0' ? app
                                                        : "włączony");
    widgets::set_power_state(screen.atv_power, atv_on, atv_fresh);
    widgets::set_status_outline(screen.atv_power, state.status_of(domain::ControlId::AtvPower));
    dim_block(screen.atv_card, atv_fresh);

    // Głośność: wartość w toku, potem potwierdzona.
    const bool z_fresh = state.z906_fresh();
    int32_t volume = snapshot.z906.volume_percent;
    if (const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ZVolume)) {
        volume = pending->requested_value;
    }
    if (z_fresh) {
        lv_label_set_text_fmt(screen.volume, "%d", static_cast<int>(volume));
    } else {
        lv_label_set_text(screen.volume, "—");
    }
    if (screen.audio_input != nullptr) {
        // Poziom: wejście ma własny kafel, przy ikonie — dekodowanie 5.1.
        lv_label_set_text(screen.audio_status, !z_fresh                    ? "brak stanu"
                                               : snapshot.z906.decode_5_1 ? "Decode 5.1"
                                               : snapshot.z906.power == domain::Power::On ? "włączony"
                                                                                            : "wyłączony");
        render_effects(context);
    } else {
        lv_label_set_text(screen.audio_status, z_fresh ? snapshot.z906.source.c_str() : "");
    }
    lv_image_set_src(screen.mute_icon, snapshot.z906.muted ? &icon_key_mute : &icon_key_volume);
    widgets::set_status_outline(screen.mute, state.status_of(domain::ControlId::ZMute));
    dim_block(screen.volume_card, z_fresh);
}

}  // namespace ui
