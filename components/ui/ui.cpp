#include "ui/ui.hpp"

#include "core/clock.hpp"
#include "core/config.hpp"
#include "core/memory.hpp"
#include "ui_internal.hpp"

namespace ui {
namespace {

// Kontekst ma kilka kilobajtów (wiersze list, stany ekranów), więc żyje w PSRAM.
Context* g_context = nullptr;

// Przyciemnienie przed wygaszeniem: jak mocno i na ile wcześniej.
constexpr int kDimPercent = 15;
constexpr uint32_t kDimLeadMs = 10000;
int64_t g_brightness_check_ms = 0;
int64_t g_theme_check_ms = 0;

// ADR 0005: stan sprzed snu nie jest stanem aktualnym. Przez czas snu ktoś mógł
// sięgnąć po pilota od wzmacniacza albo po aplikację, więc wybudzenie prosi
// o świeży stan i trzyma nakładkę łączenia, dopóki nie przyjdzie.
void wake(Context& context) {
    if (!context.asleep) {
        return;
    }
    context.asleep = false;
    lv_obj_add_flag(context.sleep_blocker, LV_OBJ_FLAG_HIDDEN);
    // ADR 0010: wybudzenie zawsze wraca na ekran startowy, a stan dociera w tle.
    // Klatka rysuje się, zanim zapali się podświetlenie — panel trzyma ostatni
    // obraz, więc inaczej na chwilę mignąłby ekran sprzed uśpienia.
    show_start(context, true);
    lv_refr_now(nullptr);
    if (context.display_power != nullptr) {
        context.display_power(true);
    }

    domain::UiIntent refresh;
    refresh.kind = domain::IntentKind::RequestSync;
    dispatch(context, refresh);
}

void doze(Context& context) {
    if (context.asleep) {
        return;
    }
    context.asleep = true;
    // Ekran startowy rysuje się już w ciemności, żeby pobudka nie czekała na pełną
    // klatkę. Wchodzi przed warstwą snu — obie leżą w lv_layer_sys, a wierzch
    // musi należeć do warstwy, inaczej dotyk w ciemno trafiłby w kafelek.
    show_start(context, true);
    // Warstwa wchodzi przed zgaszeniem, żeby dotyk w ciemno nie nacisnął niczego.
    lv_obj_remove_flag(context.sleep_blocker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(context.sleep_blocker);
    if (context.display_power != nullptr) {
        context.display_power(false);
    }
}

void build_sleep_blocker(Context& context) {
    context.sleep_blocker = lv_obj_create(lv_layer_sys());
    theme::make_flat(context.sleep_blocker);
    lv_obj_set_size(context.sleep_blocker, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(context.sleep_blocker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(context.sleep_blocker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(context.sleep_blocker);
    lv_obj_add_event_cb(
        context.sleep_blocker,
        [](lv_event_t* event) { wake(*static_cast<Context*>(lv_event_get_user_data(event))); },
        LV_EVENT_PRESSED, &context);
}

void on_state_changed(void* raw) {
    render(*static_cast<Context*>(raw));
}

void on_tick(lv_timer_t* timer) {
    auto* context = static_cast<Context*>(lv_timer_get_user_data(timer));
    // Odbiór zdarzeń źródła dzieje się w zadaniu UI, więc nic nie dotyka LVGL
    // spoza niego (ADR 0003).
    context->controller->pump();
    context->network->pump();

    // LVGL sam liczy czas od ostatniego dotyku, więc wystarczy go odpytać.
    // Czas wygaszania pochodzi z ustawień „Ekran” (domyślnie 30 s, 0 = nigdy).
    // Styl „wg godzin” przełącza się na granicy godziny, także w uśpieniu — wtedy
    // pilot budzi się już we właściwym. Sprawdzamy raz na sekundę.
    const int64_t tick_now = core::now_ms();
    if (tick_now - g_theme_check_ms >= 1000) {
        g_theme_check_ms = tick_now;
        theme::apply(effective_theme());
    }
    if (context->asleep) {
        return;
    }
    // Tryb nocny: jasność zmienia się na granicy godzin. Sprawdzamy co sekundę,
    // a płytce podajemy wartość tylko wtedy, gdy się zmieniła.
    const int64_t now = core::now_ms();
    if (!context->dimmed && (context->applied_brightness < 0 || now - g_brightness_check_ms >= 1000)) {
        g_brightness_check_ms = now;
        const int target = effective_brightness();
        if (target != context->applied_brightness && context->display_brightness != nullptr) {
            context->display_brightness(target);
            context->applied_brightness = target;
        }
        // Zegar ekranu startowego: sprawdzany co sekundę, rysowany po zmianie minuty.
        if (context->start.open) {
            render_start(*context);
        }
        // Karta „W pokoju” (ADR 0017): nowa wiadomość pogody bez nowego snapshotu.
        home::render_room_climate(*context, false);
    }
    const uint32_t timeout = screen_timeout_ms();
    const uint32_t inactive = lv_display_get_inactive_time(nullptr);
    if (timeout != 0 && inactive >= timeout) {
        context->dimmed = false;
        doze(*context);
        return;
    }
    // Przyciemnienie na 10 s przed zgaszeniem (przy krótkim czasie — w połowie).
    const bool dim_enabled = timeout != 0 && storage::enabled(storage::Pref::DimBeforeSleep);
    const uint32_t dim_at = timeout > 2 * kDimLeadMs ? timeout - kDimLeadMs : timeout / 2;
    const bool should_dim = dim_enabled && inactive >= dim_at;
    if (should_dim != context->dimmed) {
        context->dimmed = should_dim;
        if (should_dim && context->display_dim != nullptr) {
            context->display_dim(kDimPercent);
        } else if (!should_dim && context->display_power != nullptr) {
            context->display_power(true);  // wraca do jasności z ustawień
        }
    }
}

}  // namespace

void dispatch(Context& context, const domain::UiIntent& intent) {
    context.controller->dispatch(intent);
    render(context);
}

void render(Context& context) {
    render_shell(context);
    render_remote(context);
    render_tv(context);
    render_appletv(context);
    render_audio(context);
    render_home(context);
    render_more(context);
    render_start(context);
    render_wifi(context);
    render_mqtt(context);
    render_device(context);
}

void start(const Services& services) {
    // Styl przed stylami: przy jasnym z ustawień ekrany od razu powstają jasne.
    theme::apply(effective_theme());
    theme::init();
    g_context = &core::memory::make_in_psram<Context>();

    g_context->controller = services.controller;
    g_context->network = services.network;
    g_context->live = services.live;
    g_context->display_power = services.display_power;
    g_context->display_brightness = services.display_brightness;
    g_context->display_dim = services.display_dim;

    // Ustawienia „Pokój i czas”: strefa przed pierwszą synchronizacją czasu,
    // nazwa pokoju przed pierwszym rysowaniem nagłówka.
    apply_timezone();
    storage::load_room(g_context->room);

    core::memory::log_stage("przed interfejsem");
    g_context->editor = widgets::make_text_editor();
    core::memory::log_stage("klawiatura");
    build_shell(*g_context);
    core::memory::log_stage("powłoka i ekrany");
    build_start(*g_context);
    core::memory::log_stage("ekran startowy");
    build_sleep_blocker(*g_context);
    // Pod ekranem startowym czeka TV; kafelek i tak wybierze właściwy ekran.
    show_screen(*g_context, ScreenId::Tv);
    show_start(*g_context, true);

    services.controller->set_on_change(&on_state_changed, g_context);
    services.network->set_on_change(&on_state_changed, g_context);
    lv_timer_create(on_tick, 20, g_context);
}

}  // namespace ui
