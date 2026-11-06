
#include "images.h"
#include "ui_internal.hpp"

namespace ui {
namespace {

using widgets::flash;

// Trzy strefy, zawsze te same. Nowy ekran wypełnia wyłącznie strefę środkową.
struct ScreenDef {
    const char* name;
    const char* room;
    const char* symbol;
    const lv_image_dsc_t* tab_icon;
    bool has_power;
};

const ScreenDef kScreens[kScreenCount] = {
    {"LG OLED", "SALON", LV_SYMBOL_VIDEO, &icon_tv, true},
    {"Apple TV", "SALON", LV_SYMBOL_PLAY, &icon_appletv, true},
    {"Z906", "SALON", LV_SYMBOL_AUDIO, &icon_audio, true},
    {"Dom", "SALON", LV_SYMBOL_HOME, &icon_home, false},
    {"Pilot", "USTAWIENIA", LV_SYMBOL_SETTINGS, &icon_settings, false},
};

const char* const kWakeSteps[3] = {"Wi-Fi", "MQTT", "Synchronizacja"};

struct TabBinding {
    Context* context;
    ScreenId screen;
};

void on_tab(lv_event_t* event) {
    auto* binding = static_cast<TabBinding*>(lv_event_get_user_data(event));
    // Zmiana zakładki nie odpytuje sieci: pokazuje potwierdzony stan.
    show_screen(*binding->context, binding->screen);
}

void on_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    const auto& snapshot = context.controller->state().snapshot();

    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetPower;
    switch (context.screen) {
        case ScreenId::Tv:
            intent.control = domain::ControlId::TvPower;
            intent.flag = snapshot.tv.power != domain::Power::On;
            break;
        case ScreenId::AppleTv:
            intent.control = domain::ControlId::AtvPower;
            intent.flag = snapshot.appletv.power != domain::Power::On;
            break;
        case ScreenId::Audio:
            intent.control = domain::ControlId::ZPower;
            intent.flag = snapshot.z906.power != domain::Power::On;
            break;
        default:
            return;
    }
    dispatch(context, intent);
}

// Bez sieci żaden snapshot nie przyjdzie, więc wyjście musi prowadzić do całego
// ekranu WIĘCEJ, a nie wprost do brokera: stamtąd jest Wi-Fi, MQTT i tryb prób.
void on_open_settings(void* raw) {
    show_screen(*static_cast<Context*>(raw), ScreenId::More);
}

void on_retry(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::RequestSync;
    dispatch(context, intent);
}

lv_obj_t* plain(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    theme::make_flat(obj);
    return obj;
}

lv_obj_t* text_label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

int32_t gutter() {
    return theme::landscape() ? theme::kGutterWide : theme::kGutter;
}

void build_topbar(Context& context, lv_obj_t* parent) {
    Shell& shell = context.shell;
    shell.topbar = plain(parent);
    lv_obj_set_size(shell.topbar, LV_PCT(100), theme::kTopbar);
    lv_obj_set_style_pad_hor(shell.topbar, gutter(), LV_PART_MAIN);
    lv_obj_set_style_pad_top(shell.topbar, theme::kTopbarPad, LV_PART_MAIN);
    theme::make_column(shell.topbar, 0);


    lv_obj_t* identity = plain(shell.topbar);
    shell.identity = identity;
    lv_obj_set_size(identity, LV_PCT(100), theme::kIdentityRow);
    theme::make_row(identity, theme::kS3);

    lv_obj_t* mark = plain(identity);
    lv_obj_set_size(mark, 44, 44);
    lv_obj_set_style_radius(mark, theme::kR2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mark, theme::surface(), LV_PART_MAIN);
    lv_obj_set_flex_flow(mark, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mark, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    shell.ident_icon = text_label(mark, LV_SYMBOL_VIDEO, theme::font_icon_sm(), theme::accent());

    lv_obj_t* texts = plain(identity);
    lv_obj_set_height(texts, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(texts, 1);
    theme::make_column(texts, 3);

    shell.ident_name = text_label(texts, "", theme::font_b19(), theme::text());
    shell.ident_room = widgets::make_cap(texts, "");

    // Wskaźnik dekodowania 5.1 (napis „Decode”) w nagłówku ekranu dźwięku: pastylka z kropką,
    // tylko do odczytu. Zielona kropka znaczy potwierdzone dekodowanie.
    shell.decode = plain(identity);
    lv_obj_set_size(shell.decode, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_pad_hor(shell.decode, theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_radius(shell.decode, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shell.decode, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(shell.decode, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(shell.decode, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(shell.decode, theme::line(), LV_PART_MAIN);
    theme::make_row(shell.decode, theme::kS2);
    shell.decode_dot = plain(shell.decode);
    lv_obj_set_size(shell.decode_dot, 8, 8);
    lv_obj_set_style_radius(shell.decode_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shell.decode_dot, LV_OPA_COVER, LV_PART_MAIN);
    shell.decode_label = text_label(shell.decode, "Decode", theme::font_m13(), theme::dim());
    lv_obj_add_flag(shell.decode, LV_OBJ_FLAG_HIDDEN);

    // Nagłówek należy do bieżącego ekranu: zasilanie dotyczy tylko tego urządzenia,
    // które widać. Na ekranach bez urządzenia znika w ogóle.
    shell.power = widgets::make_power_button(identity, &on_power, &context);
}

void build_tabbar(Context& context) {
    Shell& shell = context.shell;
    const bool rail = theme::landscape();
    shell.tabbar = plain(shell.root);
    lv_obj_set_style_bg_opa(shell.tabbar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(shell.tabbar, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_width(shell.tabbar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(shell.tabbar, theme::line(), LV_PART_MAIN);
    if (rail) {
        // Poziom: pionowy pasek z lewej, zakładki wyśrodkowane w pionie (makieta
        // docs/ux/mockup-landscape). Kciuk lewej ręki sięga go bez przekładania pilota.
        lv_obj_set_size(shell.tabbar, theme::kRail, LV_PCT(100));
        lv_obj_set_style_border_side(shell.tabbar, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(shell.tabbar, theme::kS2, LV_PART_MAIN);
        theme::make_column(shell.tabbar, theme::kS2);
        lv_obj_set_flex_align(shell.tabbar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_size(shell.tabbar, LV_PCT(100), theme::kTabbar);
        lv_obj_set_style_border_side(shell.tabbar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
        // 4 + 72 + 4: odstęp nad i pod zaznaczoną zakładką wygląda tak samo (nad nią
        // jest jeszcze 1 px linii), a ikona 48 px siedzi w niej na środku.
        lv_obj_set_style_pad_top(shell.tabbar, theme::kS1, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(shell.tabbar, theme::kS1, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(shell.tabbar, theme::kS4, LV_PART_MAIN);
        theme::make_row(shell.tabbar, theme::kS1);
        lv_obj_set_flex_align(shell.tabbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    }

    for (std::size_t i = 0; i < kScreenCount; ++i) {
        lv_obj_t* tab = lv_button_create(shell.tabbar);
        lv_obj_remove_style_all(tab);
        if (rail) {
            lv_obj_set_size(tab, LV_PCT(100), theme::kRailTab);
        } else {
            lv_obj_set_flex_grow(tab, 1);
            lv_obj_set_height(tab, LV_PCT(100));
        }
        lv_obj_set_style_radius(tab, theme::kR3, LV_PART_MAIN);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        // Same ikony, bez podpisów (2026-11-03): sprite mówi wystarczająco, a pasek
        // oddaje 16 px treści. Sprite ma własne kolory, więc wybranej zakładki nie
        // znaczy barwa ikony, tylko jej tło. (Wcześniej próba odwrotna — same
        // podpisy — wyglądała gorzej.)
        lv_obj_t* icon = lv_image_create(tab);
        lv_image_set_src(icon, kScreens[i].tab_icon);

        auto* binding = static_cast<TabBinding*>(lv_malloc(sizeof(TabBinding)));
        binding->context = &context;
        binding->screen = static_cast<ScreenId>(i);
        lv_obj_add_event_cb(tab, on_tab, LV_EVENT_CLICKED, binding);
        lv_obj_add_event_cb(
            tab, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
            LV_EVENT_DELETE, binding);

        shell.tabs[i] = tab;
    }
}

void build_wake(Context& context) {
    Shell& shell = context.shell;

    // Warstwa nad ekranami, zdejmowana dopiero po nowym snapshocie (ADR 0005).
    shell.wake = plain(lv_layer_top());
    lv_obj_set_size(shell.wake, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(shell.wake, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(shell.wake, theme::bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(shell.wake, 48, LV_PART_MAIN);
    lv_obj_add_flag(shell.wake, LV_OBJ_FLAG_CLICKABLE);
    theme::make_column(shell.wake, 0);
    lv_obj_set_flex_align(shell.wake, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* mark = plain(shell.wake);
    lv_obj_set_size(mark, 96, 96);
    lv_obj_set_style_radius(mark, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mark, theme::accent(), LV_PART_MAIN);
    lv_obj_set_flex_flow(mark, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mark, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    shell.wake_mark = mark;
    text_label(mark, LV_SYMBOL_HOME, theme::font_icon(), theme::accent_ink());

    shell.wake_title = text_label(shell.wake, "Łączenie z domem", theme::font_b26(), theme::text());
    lv_obj_set_style_pad_top(shell.wake_title, theme::kS7, LV_PART_MAIN);

    shell.wake_note = text_label(shell.wake, "Pobieram aktualny stan TV i Z906", theme::font_r13(),
                                 theme::dim());
    lv_label_set_long_mode(shell.wake_note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(shell.wake_note, LV_PCT(100));
    lv_obj_set_style_text_align(shell.wake_note, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(shell.wake_note, theme::kS3, LV_PART_MAIN);

    lv_obj_t* steps = plain(shell.wake);
    lv_obj_set_width(steps, LV_PCT(100));
    lv_obj_set_height(steps, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(steps, theme::kS7, LV_PART_MAIN);
    theme::make_row(steps, theme::kS4);
    lv_obj_set_flex_align(steps, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 3; ++i) {
        lv_obj_t* item = plain(steps);
        lv_obj_set_height(item, LV_SIZE_CONTENT);
        lv_obj_set_width(item, LV_SIZE_CONTENT);
        theme::make_row(item, theme::kS2);

        lv_obj_t* dot = plain(item);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, theme::surface3(), LV_PART_MAIN);
        shell.wake_dots[i] = dot;

        shell.wake_steps[i] = text_label(item, kWakeSteps[i], theme::font_r11(), theme::faint());
    }

    shell.wake_retry = widgets::make_action(shell.wake, LV_SYMBOL_REFRESH, "Ponów", true, widgets::ActionLayout::Block,
                                            &on_retry, &context);
    lv_obj_set_size(shell.wake_retry, 200, theme::kTouch);
    lv_obj_set_style_margin_top(shell.wake_retry, theme::kS7, LV_PART_MAIN);

    // Bez tego pierwsze uruchomienie kończy się na nakładce: nie ma połączenia,
    // a do ustawień brokera nie da się dojść.
    shell.wake_settings = widgets::make_action(shell.wake, LV_SYMBOL_SETTINGS, "Ustawienia", false,
                                               widgets::ActionLayout::Block, &on_open_settings,
                                               &context);
    lv_obj_set_size(shell.wake_settings, 200, theme::kTouch);
    lv_obj_set_style_margin_top(shell.wake_settings, theme::kS3, LV_PART_MAIN);
}

}  // namespace

void build_shell(Context& context) {
    Shell& shell = context.shell;

    shell.root = lv_screen_active();
    lv_obj_remove_style_all(shell.root);
    lv_obj_add_style(shell.root, &theme::styles().screen, LV_PART_MAIN);
    lv_obj_remove_flag(shell.root, LV_OBJ_FLAG_SCROLLABLE);
    // Pion: nagłówek, treść, zakładki na dole. Poziom: zakładki z boku, obok
    // kolumna z nagłówkiem i treścią.
    lv_obj_t* column = shell.root;
    if (theme::landscape()) {
        theme::make_row(shell.root, 0);
        lv_obj_set_flex_align(shell.root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        build_tabbar(context);
        column = plain(shell.root);
        lv_obj_set_height(column, LV_PCT(100));
        lv_obj_set_flex_grow(column, 1);
        theme::make_column(column, 0);
    } else {
        theme::make_column(shell.root, 0);
    }

    build_topbar(context, column);

    shell.content = plain(column);
    lv_obj_set_width(shell.content, LV_PCT(100));
    lv_obj_set_flex_grow(shell.content, 1);
    lv_obj_set_style_pad_hor(shell.content, gutter(), LV_PART_MAIN);
    lv_obj_set_style_pad_top(shell.content, theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(shell.content, theme::kS4, LV_PART_MAIN);

    for (std::size_t i = 0; i < kScreenCount; ++i) {
        lv_obj_t* page = plain(shell.content);
        lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
        lv_obj_align(page, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        shell.pages[i] = page;
    }

    if (!theme::landscape()) {
        build_tabbar(context);
    }
    build_wake(context);

    build_tv(context, shell.pages[static_cast<std::size_t>(ScreenId::Tv)]);
    build_appletv(context, shell.pages[static_cast<std::size_t>(ScreenId::AppleTv)]);
    build_audio(context, shell.pages[static_cast<std::size_t>(ScreenId::Audio)]);
    build_home(context, shell.pages[static_cast<std::size_t>(ScreenId::Home)]);
    build_more(context, shell.pages[static_cast<std::size_t>(ScreenId::More)]);
    build_wifi(context);
    build_mqtt(context);
    build_device(context);
    dock_settings_pages(context);
}

namespace {

}  // namespace

void render_shell(Context& context) {
    Shell& shell = context.shell;
    const auto& state = *context.controller;
    const std::size_t index = static_cast<std::size_t>(context.screen);
    const ScreenDef& def = kScreens[index];

    lv_label_set_text(shell.ident_icon, def.symbol);
    lv_label_set_text(shell.ident_name, def.name);
    // Ekrany urządzeń noszą nazwę pokoju z ustawień; WIĘCEJ ma własny podpis.
    lv_label_set_text(shell.ident_room,
                      context.screen == ScreenId::More ? def.room : context.room.c_str());

    if (def.has_power) {
        lv_obj_remove_flag(shell.power, LV_OBJ_FLAG_HIDDEN);
        const auto& snapshot = state.state().snapshot();
        bool on = false;
        bool fresh = false;
        switch (context.screen) {
            case ScreenId::Tv:
                on = snapshot.tv.power == domain::Power::On;
                fresh = state.state().tv_fresh();
                break;
            case ScreenId::AppleTv:
                on = snapshot.appletv.power == domain::Power::On;
                fresh = state.state().appletv_fresh();
                break;
            case ScreenId::Audio:
                on = snapshot.z906.power == domain::Power::On;
                fresh = state.state().z906_fresh();
                break;
            default:
                break;
        }
        widgets::set_power_state(shell.power, on, fresh);
        widgets::set_status_outline(shell.power, state.state().status_of(
            context.screen == ScreenId::Tv      ? domain::ControlId::TvPower
            : context.screen == ScreenId::AppleTv ? domain::ControlId::AtvPower
                                                  : domain::ControlId::ZPower));
    } else {
        lv_obj_add_flag(shell.power, LV_OBJ_FLAG_HIDDEN);
    }

    // Decode 5.1 tylko na ekranie dźwięku i tylko ze świeżego stanu (ADR 0005).
    if (context.screen == ScreenId::Audio && state.state().z906_fresh()) {
        const bool decoding = state.state().snapshot().z906.decode_5_1;
        lv_obj_remove_flag(shell.decode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(shell.decode_dot, decoding ? theme::ok() : theme::faint(),
                                  LV_PART_MAIN);
        lv_obj_set_style_text_color(shell.decode_label, decoding ? theme::ok() : theme::faint(),
                                    LV_PART_MAIN);
        lv_obj_set_style_border_color(shell.decode, decoding ? theme::ok() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(shell.decode, decoding ? 120 : LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_add_flag(shell.decode, LV_OBJ_FLAG_HIDDEN);
    }

    for (std::size_t i = 0; i < kScreenCount; ++i) {
        const bool selected = i == index;
        lv_obj_set_style_bg_opa(shell.tabs[i], selected ? theme::kAccentSoftOpa : LV_OPA_TRANSP,
                                LV_PART_MAIN);
        lv_obj_set_style_bg_color(shell.tabs[i], theme::accent(), LV_PART_MAIN);
    }

    // Nakładka wybudzania: do czasu nowego snapshotu nie pokazujemy stanu urządzeń.
    // WIĘCEJ nie pokazuje stanu urządzeń, a to tam naprawia się łączność (Wi-Fi,
    // broker). Zasłonięcie go zamykało pilota bez sieci w pułapce.
    const domain::LinkPhase phase = state.state().phase();
    const bool error = phase == domain::LinkPhase::Error;
    // Ekran startowy nie pokazuje stanu sterowania, więc nakładka go nie zasłania (ADR 0010).
    if (phase == domain::LinkPhase::Ready || context.screen == ScreenId::More ||
        context.start.open) {
        lv_obj_add_flag(shell.wake, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(shell.wake, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(shell.wake);
    }

    lv_label_set_text(shell.wake_title, error ? "Brak połączenia z domem" : "Łączenie z domem");
    lv_label_set_text(shell.wake_note,
                      error ? "Nie pokazuję zapamiętanego stanu. Sprawdź ustawienia i spróbuj ponownie."
                            : "Pobieram aktualny stan TV i Z906");
    lv_obj_set_style_bg_color(shell.wake_mark, error ? theme::danger() : theme::accent(),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shell.wake_mark, error ? theme::kDangerSoftOpa : LV_OPA_COVER,
                            LV_PART_MAIN);
    lv_obj_t* wake_icon = lv_obj_get_child(shell.wake_mark, 0);
    lv_label_set_text(wake_icon, error ? LV_SYMBOL_WARNING : LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(wake_icon, error ? theme::danger() : theme::accent_ink(),
                                LV_PART_MAIN);

    const int current = static_cast<int>(state.state().step());
    for (int i = 0; i < 3; ++i) {
        lv_color_t dot = theme::surface3();
        lv_color_t text = theme::faint();
        if (i < current) {
            dot = theme::ok();
        } else if (i == current) {
            dot = error ? theme::danger() : theme::accent();
            text = error ? theme::danger() : theme::text();
        }
        lv_obj_set_style_bg_color(shell.wake_dots[i], dot, LV_PART_MAIN);
        lv_obj_set_style_text_color(shell.wake_steps[i], text, LV_PART_MAIN);
    }

    // Ponowienie ma sens dopiero po błędzie, ale wyjście do ustawień musi być
    // zawsze: inaczej nakładka zamyka użytkownika w pułapce przy każdej
    // niezgodności konfiguracji.
    if (error) {
        lv_obj_remove_flag(shell.wake_retry, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(shell.wake_retry, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(shell.wake_settings, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* make_header_slot(Context& context, int32_t width) {
    lv_obj_t* slot = plain(context.shell.identity);
    lv_obj_set_size(slot, width, LV_SIZE_CONTENT);
    theme::make_row(slot, theme::kS2);
    lv_obj_move_to_index(slot, 2);  // znak, nazwa, [miejsce], Decode, zasilanie
    lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
    return slot;
}

void show_header_slot(const Context& context, lv_obj_t* slot, ScreenId owner, bool fresh) {
    if (slot == nullptr) {
        return;
    }
    if (context.screen == owner) {
        lv_obj_remove_flag(slot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_opa(slot, fresh ? LV_OPA_COVER : 77, LV_PART_MAIN);
}

void show_screen(Context& context, ScreenId screen) {
    context.screen = screen;
    for (std::size_t i = 0; i < kScreenCount; ++i) {
        if (i == static_cast<std::size_t>(screen)) {
            lv_obj_remove_flag(context.shell.pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(context.shell.pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    sync_settings_detail(context);
    render(context);
}

}  // namespace ui
