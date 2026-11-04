#include <ctime>
#include <initializer_list>

#include "core/clock.hpp"
#include "images.h"
#include "transport/time_sync.hpp"
#include "ui_internal.hpp"

// Ekran startowy (ADR 0010): po starcie i po każdym wybudzeniu, nigdy z nawigacji.
// Układ według makiety docs/ux/mockup-pulpit (README, „Ekran startowy”).

namespace ui {
namespace {

constexpr int32_t kHeadHeight = 148;
constexpr int32_t kFootHeight = 56;
constexpr int32_t kTileGap = theme::kS3;
constexpr int32_t kTileW = (theme::kContentW - kTileGap) / 2;                  // 210
constexpr int32_t kTileH = (800 - kHeadHeight - kFootHeight - theme::kS3 - theme::kS5 -
                            kTileGap) / 2;                                    // 276

struct TileDef {
    const lv_image_dsc_t* icon;
    const char* name;
    ScreenId screen;
};

const TileDef kTiles[4] = {
    {&icon_tv_large, "Telewizor", ScreenId::Tv},
    {&icon_appletv_large, "Apple TV", ScreenId::AppleTv},
    {&icon_audio_large, "Głośniki", ScreenId::Audio},
    {&icon_home_large, "Dom", ScreenId::Home},
};

const char* const kDays[] = {"niedziela", "poniedziałek", "wtorek", "środa",
                             "czwartek",  "piątek",       "sobota"};
const char* const kMonths[] = {"stycznia", "lutego",    "marca",       "kwietnia",
                               "maja",     "czerwca",   "lipca",       "sierpnia",
                               "września", "października", "listopada", "grudnia"};

// Warunki z encji `weather` w HA (ADR 0011): ikona i polska nazwa. Nieznany warunek
// dostaje chmurę i identyfikator tak, jak przyszedł.
struct WeatherDef {
    const char* id;
    const lv_image_dsc_t* icon;
    const char* name;
    // W nocy: HA ma nocny wariant tylko dla bezchmurnego nieba (`clear-night`), więc
    // resztę rozróżnia pilot. nullptr — ten sam obraz i nazwa co w dzień.
    const lv_image_dsc_t* night_icon = nullptr;
    const char* night_name = nullptr;
};

const WeatherDef kWeather[] = {
    {"sunny", &icon_weather_sunny, "słonecznie", &icon_weather_night, "pogodnie"},
    {"clear-night", &icon_weather_night, "pogodnie"},
    {"partlycloudy", &icon_weather_partly, "przejaśnienia", &icon_weather_partly_night,
     "częściowe zachmurzenie"},
    {"cloudy", &icon_weather_cloudy, "pochmurno"},
    {"rainy", &icon_weather_rain, "deszcz"},
    {"pouring", &icon_weather_rain, "ulewa"},
    {"snowy", &icon_weather_snow, "śnieg"},
    {"snowy-rainy", &icon_weather_snow, "deszcz ze śniegiem"},
    {"hail", &icon_weather_snow, "grad"},
    {"lightning", &icon_weather_storm, "burza"},
    {"lightning-rainy", &icon_weather_storm, "burza z deszczem"},
    {"fog", &icon_weather_fog, "mgła"},
    {"windy", &icon_weather_wind, "wietrznie"},
    {"windy-variant", &icon_weather_wind, "wietrznie"},
    {"exceptional", &icon_weather_storm, "uwaga na pogodę"},
};

lv_obj_t* box(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    theme::make_flat(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t* text_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_label_set_text(label, "");
    return label;
}

lv_obj_t* dot(lv_obj_t* parent) {
    lv_obj_t* obj = box(parent);
    lv_obj_set_size(obj, 8, 8);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, theme::surface3(), LV_PART_MAIN);
    return obj;
}

// Wszystko, co mogło zostać otwarte przed snem. Inaczej podstrona z lv_layer_sys
// przykryłaby ekran urządzenia wybranego z kafelka.
void close_overlays(Context& context) {
    widgets::dismiss_confirm();
    widgets::close_text_editor(*context.editor);
    context.appletv.keyboard_open = false;
    context.appletv.keyboard_manual = false;

    widgets::show_page(context.tv.picker, false);
    context.tv.picker_control = domain::ControlId::None;
    widgets::show_page(context.tv.channel_page, false);
    context.tv.channel_page_open = false;
    widgets::show_page(context.audio.picker, false);
    context.audio.picker_control = domain::ControlId::None;

    if (context.climate.built) {
        widgets::show_page(context.climate.picker, false);
        context.climate.picker_control = domain::ControlId::None;
        widgets::show_page(context.climate.page, false);
    }

    widgets::show_page(context.wifi.page, false);
    context.wifi.open = false;
    widgets::show_page(context.mqtt.page, false);
    for (const widgets::Page* page :
         {&context.device.display, &context.device.sound, &context.device.about,
          &context.device.place, &context.device.input, &context.device.diagnostics}) {
        widgets::show_page(*page, false);
    }
}

void on_tile(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    show_start(context, false);
    show_screen(context, kTiles[index].screen);
}

void on_retry(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent refresh;
    refresh.kind = domain::IntentKind::RequestSync;
    dispatch(context, refresh);
}

// ---------- jedno zdanie stanu na kafelek ----------

struct Summary {
    char text[48];
    enum class Tone : uint8_t { Off, On, Wait, Stale } tone;
};

void summarize(const domain::AppState& state, ScreenId screen, Summary& out) {
    const domain::DeviceSnapshot& snapshot = state.snapshot();
    // ADR 0005: przed świeżym snapshotem nie mówimy nic o urządzeniach.
    if (state.phase() != domain::LinkPhase::Ready) {
        const bool error = state.phase() == domain::LinkPhase::Error;
        lv_snprintf(out.text, sizeof(out.text), "%s", error ? "brak połączenia" : "łączenie…");
        out.tone = error ? Summary::Tone::Stale : Summary::Tone::Wait;
        return;
    }

    bool fresh = false;
    switch (screen) {
        case ScreenId::Tv: fresh = snapshot.tv.available; break;
        case ScreenId::AppleTv: fresh = snapshot.appletv.available; break;
        case ScreenId::Audio: fresh = snapshot.z906.available; break;
        default: fresh = snapshot.climate.available; break;
    }
    if (!fresh) {
        lv_snprintf(out.text, sizeof(out.text), "brak świeżego stanu");
        out.tone = Summary::Tone::Stale;
        return;
    }

    out.tone = Summary::Tone::On;
    switch (screen) {
        case ScreenId::Tv: {
            const domain::TvState& tv = snapshot.tv;
            if (tv.power != domain::Power::On) {
                lv_snprintf(out.text, sizeof(out.text), "wyłączony");
                out.tone = Summary::Tone::Off;
            } else if (!tv.picture_mode.empty()) {
                lv_snprintf(out.text, sizeof(out.text), "%s · %s", tv.source.c_str(),
                            tv.picture_mode.c_str());
            } else {
                lv_snprintf(out.text, sizeof(out.text), "%s", tv.source.c_str());
            }
            return;
        }
        case ScreenId::AppleTv: {
            const domain::AppleTvState& atv = snapshot.appletv;
            if (atv.power != domain::Power::On) {
                lv_snprintf(out.text, sizeof(out.text), "uśpiony");
                out.tone = Summary::Tone::Off;
                return;
            }
            const char* app = "ekran główny";
            for (std::size_t i = 0; i < atv.app_count; ++i) {
                if (!atv.app_id.empty() && atv.apps[i].id == atv.app_id) {
                    app = atv.apps[i].label.c_str();
                }
            }
            lv_snprintf(out.text, sizeof(out.text), "%s", app);
            return;
        }
        case ScreenId::Audio: {
            const domain::Z906State& z906 = snapshot.z906;
            if (z906.power != domain::Power::On) {
                lv_snprintf(out.text, sizeof(out.text), "wyłączone");
                out.tone = Summary::Tone::Off;
            } else if (z906.muted) {
                lv_snprintf(out.text, sizeof(out.text), "wyciszone · %s", z906.source.c_str());
            } else {
                lv_snprintf(out.text, sizeof(out.text), "głośność %d · %s",
                            static_cast<int>(z906.volume_percent), z906.source.c_str());
            }
            return;
        }
        default: {
            const domain::ClimateState& climate = snapshot.climate;
            if (!climate.is_on()) {
                lv_snprintf(out.text, sizeof(out.text), "klima wyłączona");
                out.tone = Summary::Tone::Off;
            } else if (climate.uses_target()) {
                char temp[16];
                widgets::format_tenths(temp, sizeof(temp), climate.target_tenths);
                lv_snprintf(out.text, sizeof(out.text), "klima %s° · %s", temp,
                            climate_mode_name(climate.hvac_mode.c_str()));
            } else {
                lv_snprintf(out.text, sizeof(out.text), "klima · %s",
                            climate_mode_name(climate.hvac_mode.c_str()));
            }
            return;
        }
    }
}

// Pogoda z pamięci źródła: pokazana od razu po wybudzeniu, bez synchronizacji (ADR 0011).
// Starsza niż kWeatherMaxAgeMs znika, zamiast udawać aktualną.
void render_weather(Context& context) {
    StartScreen& screen = context.start;
    const domain::Weather* weather = context.controller->weather();
    const bool show = weather != nullptr && weather->known &&
                      core::now_ms() - weather->received_ms < domain::kWeatherMaxAgeMs;
    if (!show) {
        if (!lv_obj_has_flag(screen.weather, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(screen.weather, LV_OBJ_FLAG_HIDDEN);
        }
        screen.shown_weather = 0;
        return;
    }
    if (screen.shown_weather == weather->revision) {
        return;  // bez zmian: nie przerysowujemy co sekundę
    }
    screen.shown_weather = weather->revision;

    const WeatherDef* def = nullptr;
    for (const WeatherDef& entry : kWeather) {
        if (weather->condition == entry.id) {
            def = &entry;
        }
    }
    const bool night = def != nullptr && weather->night && def->night_icon != nullptr;
    lv_image_set_src(screen.weather_icon, def == nullptr ? &icon_weather_cloudy
                                          : night        ? def->night_icon
                                                         : def->icon);
    // Do pełnego stopnia: na ekranie startowym dziesiąte części to szum.
    const int whole = (weather->temperature_tenths + (weather->temperature_tenths >= 0 ? 5 : -5)) / 10;
    lv_label_set_text_fmt(screen.weather_temp, "%d°C", whole);
    lv_label_set_text(screen.weather_text, def == nullptr ? weather->condition.c_str()
                                           : night        ? def->night_name
                                                          : def->name);
    lv_obj_remove_flag(screen.weather, LV_OBJ_FLAG_HIDDEN);
}

void render_clock(Context& context) {
    StartScreen& screen = context.start;
    if (!transport::time_known()) {
        // Bez czasu z sieci kreski zamiast zmyślonej godziny, jak w pasku statusu.
        if (screen.shown_minute != -2) {
            lv_label_set_text(screen.time, "—:—");
            lv_label_set_text(screen.date, context.room.c_str());
            screen.shown_minute = -2;
        }
        return;
    }
    const std::time_t now = std::time(nullptr);
    std::tm parts = {};
    localtime_r(&now, &parts);
    const int minute = parts.tm_yday * 1440 + parts.tm_hour * 60 + parts.tm_min;
    if (minute == screen.shown_minute) {
        return;
    }
    screen.shown_minute = minute;
    lv_label_set_text_fmt(screen.time, "%d:%02d", parts.tm_hour, parts.tm_min);
    lv_label_set_text_fmt(screen.date, "%s, %d %s · %s", kDays[parts.tm_wday % 7], parts.tm_mday,
                          kMonths[parts.tm_mon % 12], context.room.c_str());
}

}  // namespace

void build_start(Context& context) {
    StartScreen& screen = context.start;

    screen.root = box(lv_layer_sys());
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_CLICKABLE);  // nic pod spodem nie dostaje dotyku
    lv_obj_set_size(screen.root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(screen.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.root, theme::bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.root, theme::kGutter, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(screen.root, theme::kS5, LV_PART_MAIN);
    theme::make_column(screen.root, 0);

    // Nagłówek: zegar i data z nazwą pokoju po lewej, pogoda po prawej.
    lv_obj_t* head = box(screen.root);
    lv_obj_set_size(head, LV_PCT(100), kHeadHeight);
    theme::make_row(head, theme::kS3);

    lv_obj_t* clock = box(head);
    lv_obj_set_height(clock, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(clock, 1);
    theme::make_column(clock, theme::kS2);
    screen.time = text_label(clock, theme::font_b64num(), theme::text());
    screen.date = text_label(clock, theme::font_r13(), theme::dim());

    screen.weather = box(head);
    lv_obj_set_size(screen.weather, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_row(screen.weather, theme::kS2);
    screen.weather_icon = lv_image_create(screen.weather);
    lv_obj_t* weather_texts = box(screen.weather);
    lv_obj_set_size(weather_texts, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    theme::make_column(weather_texts, 4);
    lv_obj_set_flex_align(weather_texts, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);
    screen.weather_temp = text_label(weather_texts, theme::font_b26(), theme::text());
    screen.weather_text = text_label(weather_texts, theme::font_r13(), theme::dim());
    lv_obj_set_style_max_width(screen.weather_text, 140, LV_PART_MAIN);
    lv_label_set_long_mode(screen.weather_text, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(screen.weather, LV_OBJ_FLAG_HIDDEN);

    // Cztery kafelki 2 × 2.
    lv_obj_t* grid = box(screen.root);
    lv_obj_set_size(grid, LV_PCT(100), 2 * kTileH + kTileGap);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(grid, kTileGap, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, kTileGap, LV_PART_MAIN);

    for (std::size_t i = 0; i < 4; ++i) {
        StartScreen::Tile& tile = screen.tiles[i];
        tile.root = lv_button_create(grid);
        lv_obj_remove_style_all(tile.root);
        lv_obj_add_style(tile.root, &theme::styles().tile, LV_PART_MAIN);
        lv_obj_set_size(tile.root, kTileW, kTileH);
        lv_obj_set_style_radius(tile.root, theme::kR4, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tile.root, theme::kS5, LV_PART_MAIN);
        theme::make_column(tile.root, 0);
        lv_obj_set_flex_align(tile.root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);

        // Sprite 24 × 24 w skali 4 ×, wygenerowany bez wygładzania (tools/gen_assets.py).
        lv_obj_t* icon = lv_image_create(tile.root);
        lv_image_set_src(icon, kTiles[i].icon);

        lv_obj_t* texts = box(tile.root);
        lv_obj_set_size(texts, LV_PCT(100), LV_SIZE_CONTENT);
        theme::make_column(texts, 6);
        lv_obj_t* name = text_label(texts, theme::font_b26(), theme::text());
        lv_label_set_text(name, kTiles[i].name);

        lv_obj_t* row = box(texts);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        theme::make_row(row, 6);
        tile.dot = dot(row);
        tile.state = text_label(row, theme::font_r13(), theme::faint());
        lv_obj_set_flex_grow(tile.state, 1);
        lv_label_set_long_mode(tile.state, LV_LABEL_LONG_DOT);

        // Kafelki są jedynymi dziećmi siatki, więc ich kolejność to indeks w `kTiles`.
        lv_obj_add_event_cb(
            tile.root,
            [](lv_event_t* event) {
                auto* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
                on_tile(lv_event_get_user_data(event),
                        static_cast<std::size_t>(lv_obj_get_index(target)));
            },
            LV_EVENT_CLICKED, &context);
    }

    // Stopka: stan łącza z domem i Ponów po błędzie.
    lv_obj_t* foot = box(screen.root);
    lv_obj_set_size(foot, LV_PCT(100), kFootHeight);
    lv_obj_set_style_margin_top(foot, theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(foot, theme::kS1, LV_PART_MAIN);
    theme::make_row(foot, theme::kS2);
    screen.link_dot = dot(foot);
    screen.link_text = text_label(foot, theme::font_r13(), theme::faint());
    lv_obj_set_flex_grow(screen.link_text, 1);
    screen.retry = widgets::make_action(foot, LV_SYMBOL_REFRESH, "Ponów", true,
                                        widgets::ActionLayout::Fill, &on_retry, &context);
    lv_obj_set_flex_grow(screen.retry, 0);
    lv_obj_set_size(screen.retry, 132, 48);

    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
}

void show_start(Context& context, bool visible) {
    StartScreen& screen = context.start;
    if (screen.root == nullptr) {
        return;
    }
    screen.open = visible;
    if (visible) {
        close_overlays(context);
        screen.shown_minute = -1;
        screen.shown_weather = 0;  // po wybudzeniu sprawdzamy wiek pogody od nowa
        lv_obj_remove_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(screen.root);
    } else {
        lv_obj_add_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
    }
    render(context);
}

void render_start(Context& context) {
    StartScreen& screen = context.start;
    if (screen.root == nullptr || !screen.open) {
        return;
    }
    render_clock(context);
    render_weather(context);

    const domain::AppState& state = context.controller->state();
    Summary summary{};
    for (std::size_t i = 0; i < 4; ++i) {
        summarize(state, kTiles[i].screen, summary);
        const StartScreen::Tile& tile = screen.tiles[i];
        lv_label_set_text(tile.state, summary.text);
        lv_color_t dot_color = theme::surface3();
        lv_color_t text_color = theme::faint();
        switch (summary.tone) {
            case Summary::Tone::On:
                dot_color = theme::accent();
                text_color = theme::dim();
                break;
            case Summary::Tone::Wait: dot_color = theme::faint(); break;
            case Summary::Tone::Stale: dot_color = theme::danger(); break;
            case Summary::Tone::Off: break;
        }
        lv_obj_set_style_bg_color(tile.dot, dot_color, LV_PART_MAIN);
        lv_obj_set_style_text_color(tile.state, text_color, LV_PART_MAIN);
    }

    const domain::LinkPhase phase = state.phase();
    const bool ready = phase == domain::LinkPhase::Ready;
    const bool error = phase == domain::LinkPhase::Error;
    lv_label_set_text(screen.link_text, ready   ? "Połączono z domem"
                                        : error ? "Brak połączenia z domem"
                                                : "Łączenie z domem…");
    lv_obj_set_style_text_color(screen.link_text, ready ? theme::dim() : theme::faint(),
                                LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.link_dot,
                              ready ? theme::ok() : error ? theme::danger() : theme::faint(),
                              LV_PART_MAIN);
    if (error) {
        lv_obj_remove_flag(screen.retry, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen.retry, LV_OBJ_FLAG_HIDDEN);
    }
}

}  // namespace ui
