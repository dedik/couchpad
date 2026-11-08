#pragma once

// Zakładka Dom (ADR 0014), części wspólne dla jej plików: katalog pomieszczeń i efektów,
// dane z lights, zamiary i budowa podstrony. Prywatne dla komponentu `ui`.
//
//   screen_home.cpp   siatka pomieszczeń i klimatyzacja na ekranie Dom
//   home_room.cpp     podstrona pomieszczenia i stos poziomów
//   home_light.cpp    światło: grupa, żarówka, lista efektów
//   home_drawers.cpp  poziom: szuflady przy krawędziach
//   home_common.cpp   katalog i dane wspólne

#include <cstddef>
#include <cstdint>

#include "../ui_internal.hpp"

namespace ui::home {

using Level = RoomScreen::Level;

struct RoomDef {
    const char* id;      // identyfikator `room` w sekcji `lights`
    const char* name;
    const char* caps;    // wersalikami, do podpisu pod tytułem
    const lv_image_dsc_t* icon;    // 48 px: pasek pomieszczeń
    const lv_image_dsc_t* large;   // 96 px: kafelek
};

extern const RoomDef kRooms[kRoomCount];

// Klimatyzacja z kontraktu (ADR 0009) wisi w salonie.
inline constexpr std::size_t kClimateRoom = 0;

// Efekty WiZ: polska nazwa i kolejność na liście. Nieznany efekt pokazujemy tak, jak przyszedł,
// na końcu listy.
struct EffectDef {
    const char* id;
    const char* label;
    const char* icon;   // znak z font_effect_icon() (Lucide), jak w makiecie
};

inline constexpr std::size_t kEffectCount = 16;
extern const EffectDef kEffectNames[kEffectCount];

// Ikona (znak font_effect_icon) i polska nazwa efektu; pusty identyfikator to „bez efektu”.
const char* effect_icon(const char* id);
const char* effect_label(const char* id);

// Barwa: od ciepłej do zimnej, stałe kolory poza paletą (nie zmieniają się ze stylem).
extern const uint32_t kKelvinColors[kKelvinSteps];

inline constexpr int32_t kStripHeight = 56;
inline constexpr int32_t kChip = 64;
inline constexpr int32_t kCardHeight = 124;
inline constexpr int32_t kRailItem = 64;

// ---------- kształty ----------
lv_obj_t* card(lv_obj_t* parent);
// Ciepłe tło lampki i studzienki: 0 — wyłączona, 1 — przygaszona, 2 — świeci.
void paint_glow(lv_obj_t* obj, int glow);

// ---------- dane ----------
const domain::RoomLight* room_light(const domain::AppState& state, std::size_t room);
// Stan lampki: 2 — świeci (≥ 50 %), 1 — przygaszona, 0 — wyłączona.
int glow_of(const domain::RoomLight& room, const domain::LightState& light);
const lv_image_dsc_t* lamp_icon(bool fresh, const domain::LightState& light, int glow);
// Światło, którym steruje bieżący widok: grupa (poziom „Light”) albo żarówka.
const domain::LightState* current_light(const Context& context, const domain::RoomLight& room);
bool pending_for(const domain::AppState& state, domain::ControlId control, const char* id);
domain::ControlStatus status_for(const domain::AppState& state, domain::ControlId control, const char* id);
void climate_summary(char* out, std::size_t size, const domain::AppState& state);

// ---------- zamiary ----------
void send_power(Context& context, const domain::LightState& light);
void on_light_power(void* raw);

// ---------- podstrona pomieszczenia (home_room.cpp) ----------
void push(Context& context, Level level);
void open_climate(lv_event_t* event);
lv_obj_t* make_rail(lv_obj_t* parent, Context& context, bool wide);
void open_room(Context& context, std::size_t index);
void render_room(Context& context);

// ---------- światło (home_light.cpp) ----------
void build_light_view(Context& context, bool wide);
void build_effect_picker(Context& context);
void render_light_view(Context& context, const domain::RoomLight& room, bool group);
void render_effect_picker(Context& context, const domain::RoomLight& room);
void open_effect_picker(Context& context);

// ---------- szuflady w poziomie (home_drawers.cpp) ----------
// `side`: -1 zakładki, 1 pokoje, 0 obie schowane.
void set_drawer(Context& context, int8_t side, bool animate);
void build_drawers(Context& context);

}  // namespace ui::home
