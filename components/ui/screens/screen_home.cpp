#include "ui_internal.hpp"

namespace ui {
namespace {

// Sceny nie mają jeszcze odpowiedników w kontrakcie MQTT, więc są poleceniami
// chwilowymi: nie ma czego potwierdzać, jest tylko odzew dotyku.
const widgets::TileSpec kScenes[4] = {
    {nullptr, LV_SYMBOL_VIDEO, "Film"},
    {nullptr, LV_SYMBOL_AUDIO, "Muzyka"},
    {nullptr, LV_SYMBOL_EYE_CLOSE, "Dobranoc"},
    {nullptr, LV_SYMBOL_POWER, "Wszystko off"},
};

const char* const kSceneIds[4] = {"film", "muzyka", "noc", "koniec"};

void on_scene(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::Momentary;
    intent.control = domain::ControlId::Scene;
    intent.text.assign(kSceneIds[index]);
    dispatch(context, intent);
}

}  // namespace

void build_home(Context& context, lv_obj_t* parent) {
    HomeScreen& screen = context.home;
    screen.section = widgets::make_section(parent);

    if (theme::landscape()) {
        // Poziom: kafelek klimatyzacji po lewej na pełną wysokość, sceny 2 × 2 obok.
        // Treść ma 383 px wysokości: podpis i dwa rzędy scen po 166 px.
        widgets::make_split(screen.section, theme::kS4);

        lv_obj_t* left = widgets::make_pane(screen.section, 300, theme::kS3);
        widgets::make_cap(left, "KLIMAT");
        build_climate_tile(context, left);
        lv_obj_set_flex_grow(context.climate.tile, 1);

        lv_obj_t* right = widgets::make_pane(screen.section, 0, theme::kS3);
        widgets::make_cap(right, "SCENY");
        screen.scenes = widgets::make_tiles(right, kScenes, 4, 2, 166, &on_scene, &context);
        return;
    }

    widgets::make_cap(screen.section, "KLIMAT");
    build_climate_tile(context, screen.section);

    widgets::make_cap(screen.section, "SCENY");
    screen.scenes = widgets::make_tiles(screen.section, kScenes, 4, 2, 120, &on_scene, &context);
}

void render_home(Context& context) {
    render_climate(context);
}

}  // namespace ui
