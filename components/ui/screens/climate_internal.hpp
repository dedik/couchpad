#pragma once

// Klimatyzacja (ADR 0009): części wspólne kafelka na ekranie Dom (climate_tile.cpp)
// i pełnoekranowej podstrony (screen_climate.cpp). Prywatne dla komponentu `ui`.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../ui_internal.hpp"
#include "images.h"

namespace ui::climate {

inline constexpr int32_t kTileHeight = 168;
inline constexpr int32_t kOptionHeight = 88;
// Poziom: kafel trybu i pole nawiewu na podstronie (wariant P1).
inline constexpr int32_t kWideModeHeight = 72;
// Siatka trybów: wybrany tryb to duży kafel w pierwszej kolumnie na wszystkie rzędy,
// pozostałe po dwa w rzędzie obok. Rzędów 2–4: encja ma najwyżej osiem trybów bez `off`.
inline constexpr int32_t kModeCols[] = {LV_GRID_FR(6), LV_GRID_FR(5), LV_GRID_FR(5), LV_GRID_TEMPLATE_LAST};
inline constexpr int32_t kModeRows2[] = {kWideModeHeight, kWideModeHeight, LV_GRID_TEMPLATE_LAST};
inline constexpr int32_t kModeRows3[] = {kWideModeHeight, kWideModeHeight, kWideModeHeight,
                              LV_GRID_TEMPLATE_LAST};
inline constexpr int32_t kModeRows4[] = {kWideModeHeight, kWideModeHeight, kWideModeHeight, kWideModeHeight,
                              LV_GRID_TEMPLATE_LAST};
inline constexpr int32_t kWideOptionHeight = 68;

// Encja podaje identyfikatory HA. Nazwy do pokazania dobiera ekran; nieznany
// identyfikator pokazujemy tak, jak przyszedł, zamiast go ukrywać.
struct Name {
    const char* id;
    const char* label;
};

inline constexpr Name kModeNames[] = {
    {"off", "Wyłączona"},  {"heat_cool", "Auto"},     {"auto", "Auto"},
    {"cool", "Chłodzenie"}, {"heat", "Grzanie"},     {"dry", "Osuszanie"},
    {"fan_only", "Wentylator"},
};

// Ikona kafla trybu (poziom): pixel art z tools/draw_option_icons.py, 24 px na małym
// kaflu, 48 px na dużym kaflu wybranego trybu.
struct ModeIcon {
    const char* id;
    const lv_image_dsc_t* icon;
    const lv_image_dsc_t* large;
};
inline const ModeIcon kModeIcons[] = {
    {"heat_cool", &icon_mode_auto, &icon_mode_auto_large}, {"auto", &icon_mode_auto, &icon_mode_auto_large},
    {"cool", &icon_mode_cool, &icon_mode_cool_large},       {"heat", &icon_mode_heat, &icon_mode_heat_large},
    {"dry", &icon_mode_dry, &icon_mode_dry_large},          {"fan_only", &icon_mode_fan_only, &icon_mode_fan_only_large},
};
inline const lv_image_dsc_t* mode_icon(const char* id, bool large) {
    for (const ModeIcon& item : kModeIcons) {
        if (std::strcmp(item.id, id) == 0) {
            return large ? item.large : item.icon;
        }
    }
    return large ? &icon_mode_auto_large : &icon_mode_auto;
}

// Podpis nad temperaturą na tarczy, wersalikami jak każda etykieta.
inline constexpr Name kModeActions[] = {
    {"off", "WYŁĄCZONA"},  {"heat_cool", "AUTO DO"},      {"auto", "AUTO DO"},
    {"cool", "CHŁODZENIE DO"}, {"heat", "GRZANIE DO"},    {"dry", "OSUSZANIE DO"},
    {"fan_only", "SAM NAWIEW"},
};

// Biegi `fan_modes`: standardowe z HA i te, które podaje integracja Midea.
inline constexpr Name kFanNames[] = {
    {"auto", "Auto"},        {"quiet", "Cicha"},           {"low", "Niska"},
    {"medlow", "Średnio niska"}, {"medium", "Średnia"},   {"middle", "Średnia"},
    {"medhigh", "Średnio wysoka"}, {"high", "Wysoka"},    {"strong", "Maksymalna"},
    {"top", "Maksymalna"},   {"on", "Włączona"},          {"off", "Wyłączona"},
};

inline constexpr Name kSwingNames[] = {
    {"off", "Stałe"},  {"on", "Ruch"},      {"Full", "Ruch"},     {"both", "Oba kierunki"},
    {"vertical", "Pion"}, {"horizontal", "Poziom"}, {"Upper", "Góra"}, {"Lower", "Dół"},
    {"Left", "Lewo"},  {"Center", "Środek"}, {"Right", "Prawo"},
};

template <std::size_t N>
inline const char* name_of(const Name (&names)[N], const char* id) {
    for (const Name& name : names) {
        if (std::strcmp(name.id, id) == 0) {
            return name.label;
        }
    }
    return id;
}

// Temperatura docelowa w toku, potem potwierdzona.
int32_t shown_target(const domain::AppState& state);

// Zamiary i podstrona (screen_climate.cpp).
void on_power(void* raw);
void on_target(void* raw, int32_t tenths, bool);
void open_picker(Context& context, domain::ControlId control);
void on_tile(lv_event_t* event);
void build_page(Context& context);
void render_page(Context& context);

// Kafelek (climate_tile.cpp).
void render_tile(Context& context);

}  // namespace ui::climate
