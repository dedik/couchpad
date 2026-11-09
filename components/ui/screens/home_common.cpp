#include <cstdio>
#include <cstring>

#include "images.h"
#include "core/clock.hpp"
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

#define FX(id, label, icon) {id, label, &icon_fx_##icon, &icon_fx_##icon##_large}
const EffectDef kEffectNames[kEffectCount] = {
    FX("Cozy", "Przytulnie", cozy),
    FX("Relax", "Relaks", relax),
    FX("TV time", "Seans TV", tv),
    FX("Candlelight", "Świeca", candle),
    FX("Warm white", "Ciepła biel", warm),
    FX("Golden white", "Złota biel", golden),
    FX("Daylight", "Dzienne", daylight),
    FX("Cool white", "Zimna biel", cool),
    FX("Focus", "Skupienie", focus),
    FX("Night light", "Nocne", night),
    FX("Bedtime", "Do snu", bedtime),
    FX("Wake-up", "Pobudka", wakeup),
    FX("Pulse", "Puls", pulse),
    FX("Steampunk", "Steampunk", steampunk),
    FX("Diwali", "Diwali", diwali),
    FX("Alarm", "Alarm", alarm),
};
#undef FX

const lv_image_dsc_t* effect_icon(const char* id, bool large) {
    if (id[0] == '\0') {
        return large ? &icon_fx_none_large : &icon_fx_none;
    }
    for (const EffectDef& def : kEffectNames) {
        if (std::strcmp(def.id, id) == 0) {
            return large ? def.large : def.icon;
        }
    }
    // Efekt spoza tabeli: iskry, jak tryb Auto.
    return large ? &icon_mode_auto_large : &icon_mode_auto;
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
    return state.pending_of(control, id) != nullptr;
}

domain::ControlStatus status_for(const domain::AppState& state, domain::ControlId control,
                                 const char* id) {
    return state.status_of(control, id);
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

bool room_temperature(const Context& context, std::size_t room, int16_t& tenths) {
    const domain::Weather* weather = context.controller->weather();
    if (weather == nullptr || !weather->known || room >= kRoomCount ||
        core::now_ms() - weather->received_ms >= domain::kWeatherMaxAgeMs) {
        return false;
    }
    const domain::RoomReading* reading = domain::find_room_reading(*weather, kRooms[room].id);
    if (reading == nullptr || !reading->temperature_known) {
        return false;
    }
    tenths = reading->temperature_tenths;
    return true;
}

}  // namespace ui::home
