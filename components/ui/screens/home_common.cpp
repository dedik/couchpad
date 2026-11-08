#include <cstdio>
#include <cstring>

#include "images.h"
#include "home_internal.hpp"

// Zakładka Dom: katalog pomieszczeń i efektów oraz dane wspólne dla jej plików.

namespace ui::home {

using widgets::make_box;
using widgets::make_chevron;
using widgets::make_text;
using widgets::set_visible;

const RoomDef kRooms[kRoomCount] = {
    {"salon", "Salon", "SALON", &icon_room_salon, &icon_room_salon_large},
    {"kuchnia", "Kuchnia", "KUCHNIA", &icon_room_kuchnia, &icon_room_kuchnia_large},
    {"przedpokoj", "Przedpokój", "PRZEDPOKÓJ", &icon_room_przedpokoj, &icon_room_przedpokoj_large},
    {"biuro", "Biuro", "BIURO", &icon_room_biuro, &icon_room_biuro_large},
    {"lazienka", "Łazienka", "ŁAZIENKA", &icon_room_lazienka, &icon_room_lazienka_large},
    {"sypialnia", "Sypialnia", "SYPIALNIA", &icon_room_sypialnia, &icon_room_sypialnia_large},
};

const EffectDef kEffectNames[kEffectCount] = {
    {"Cozy", "Przytulnie", "\xEE\x8B\x84"},
    {"Relax", "Relaks", "\xEE\x8B\x9E"},
    {"TV time", "Seans TV", "\xEE\x86\x95"},
    {"Candlelight", "Świeca", "\xEE\x83\x97"},
    {"Warm white", "Ciepła biel", "\xEE\x8A\x99"},
    {"Golden white", "Złota biel", "\xEE\x8A\xB1"},
    {"Daylight", "Dzienne", "\xEE\x85\xBC"},
    {"Cool white", "Zimna biel", "\xEE\x85\xA9"},
    {"Focus", "Skupienie", "\xEE\x86\x84"},
    {"Night light", "Nocne", "\xEE\x84\xA2"},
    {"Bedtime", "Do snu", "\xEE\x8B\x81"},
    {"Wake-up", "Pobudka", "\xEE\x85\xBD"},
    {"Pulse", "Puls", "\xEE\x80\xB8"},
    {"Steampunk", "Steampunk", "\xEE\x8C\x8B"},
    {"Diwali", "Diwali", "\xEE\x90\x97"},
    {"Alarm", "Alarm", "\xEE\x8B\xAF"},
};
constexpr const char* kNoEffectIcon = "\xEE\x90\x86";   // circle-off
// Efekt spoza tabeli: iskierki, jak Diwali.
constexpr const char* kUnknownEffectIcon = "\xEE\x90\x97";

const char* effect_icon(const char* id) {
    if (id[0] == '\0') {
        return kNoEffectIcon;
    }
    for (const EffectDef& def : kEffectNames) {
        if (std::strcmp(def.id, id) == 0) {
            return def.icon;
        }
    }
    return kUnknownEffectIcon;
}

const char* effect_label(const char* id) {
    for (const EffectDef& def : kEffectNames) {
        if (std::strcmp(def.id, id) == 0) {
            return def.label;
        }
    }
    return id;
}

const uint32_t kKelvinColors[kKelvinSteps] = {0xffbf75, 0xffd291, 0xffe2b5, 0xfff0da,
                                               0xf6f6f6, 0xe3ecff, 0xcddcff};


lv_obj_t* card(lv_obj_t* parent) {
    lv_obj_t* obj = widgets::make_bare_button(parent);
    lv_obj_add_style(obj, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, theme::kR4, LV_PART_MAIN);
    return obj;
}

// Ciepłe tło lampki i studzienki: włączona mocno, przygaszona słabiej.
void paint_glow(lv_obj_t* obj, int glow) {
    if (glow == 0) {
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj, theme::surface2(), LV_PART_MAIN);
        lv_obj_set_style_border_color(obj, theme::line(), LV_PART_MAIN);
        lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        return;
    }
    lv_obj_set_style_bg_color(obj, theme::accent(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, glow == 2 ? 92 : 46, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, theme::accent(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(obj, glow == 2 ? LV_OPA_COVER : 128, LV_PART_MAIN);
}

const domain::RoomLight* room_light(const domain::AppState& state, std::size_t room) {
    return state.snapshot().lights.room(kRooms[room].id);
}

// Stan lampki: 2 — świeci (≥ 50 %), 1 — przygaszona, 0 — wyłączona.
int glow_of(const domain::RoomLight& room, const domain::LightState& light) {
    if (!light.on) {
        return 0;
    }
    return room.dimmable && light.brightness < 50 ? 1 : 2;
}

const lv_image_dsc_t* lamp_icon(bool fresh, const domain::LightState& light, int glow) {
    if (!fresh || !light.available) {
        return &icon_lamp_na;
    }
    return glow == 2 ? &icon_lamp_on : glow == 1 ? &icon_lamp_dim : &icon_lamp_off;
}

// Światło, którym steruje bieżący widok: grupa (poziom „Light”) albo żarówka.
const domain::LightState* current_light(const Context& context, const domain::RoomLight& room) {
    const RoomScreen& screen = context.room_page;
    const Level level = screen.depth > 0 ? screen.stack[screen.depth - 1] : Level::Room;
    if (level == Level::Bulb) {
        return screen.bulb < room.member_count ? &room.members[screen.bulb] : nullptr;
    }
    return &room.main;
}

bool pending_for(const domain::AppState& state, domain::ControlId control, const char* id) {
    const domain::PendingEntry* pending = state.pending_of(control);
    return pending != nullptr && pending->requested == id;
}

domain::ControlStatus status_for(const domain::AppState& state, domain::ControlId control,
                                 const char* id) {
    return pending_for(state, control, id) ? state.status_of(control)
                                           : domain::ControlStatus::Idle;
}

void send_power(Context& context, const domain::LightState& light) {
    if (!context.controller->state().lights_fresh() || !light.available) {
        return;
    }
    domain::UiIntent intent = make_intent(domain::IntentKind::SetPower, domain::ControlId::LightPower);
    intent.flag = !light.on;
    intent.target.assign(light.id.c_str());
    intent.text.assign(light.id.c_str());
    dispatch(context, intent);
}

void climate_summary(char* out, std::size_t size, const domain::AppState& state) {
    const domain::ClimateState& climate = state.snapshot().climate;
    if (!state.climate_fresh()) {
        std::snprintf(out, size, "brak świeżego stanu");
        return;
    }
    if (climate.is_on() && climate.uses_target()) {
        char temp[16];
        widgets::format_tenths(temp, sizeof(temp), climate.target_tenths);
        std::snprintf(out, size, "%s · %s°", climate_mode_name(climate.hvac_mode.c_str()), temp);
    } else {
        std::snprintf(out, size, "%s", climate_mode_name(climate.hvac_mode.c_str()));
    }
}

}  // namespace ui::home
