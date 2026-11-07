#include <cstdio>
#include <cstring>

#include "images.h"
#include "ui_internal.hpp"

// Ekran Dom: pomieszczenia (ADR 0014). Układ według makiet docs/ux/mockup-pulpit (pion)
// i docs/ux/mockup-landscape (poziom); wspólna logika makiet: docs/ux/mockup-dom/rooms.js.
//
// Dom — siatka sześciu pomieszczeń jak ekran startowy; lampka w rogu kafelka przełącza
// główne światło pokoju. Dotknięcie kafelka otwiera pokój: jego urządzenia (światło,
// w salonie także klimatyzacja), dalej światło z jasnością, barwą, efektem całej grupy
// i listą żarówek, a z niej pojedynczą żarówkę.
//
// Pilot nie zna encji. Światła przychodzą w sekcji `lights` snapshotu z krótkimi
// identyfikatorami i nimi idą polecenia `light.*` — encje zna wyłącznie bridge.

namespace ui {
namespace {

using Level = RoomScreen::Level;

struct RoomDef {
    const char* id;      // identyfikator `room` w sekcji `lights`
    const char* name;
    const char* caps;    // wersalikami, do podpisu pod tytułem
    const lv_image_dsc_t* icon;    // 48 px: pasek pomieszczeń
    const lv_image_dsc_t* large;   // 96 px: kafelek
};

const RoomDef kRooms[kRoomCount] = {
    {"salon", "Salon", "SALON", &icon_room_salon, &icon_room_salon_large},
    {"kuchnia", "Kuchnia", "KUCHNIA", &icon_room_kuchnia, &icon_room_kuchnia_large},
    {"przedpokoj", "Przedpokój", "PRZEDPOKÓJ", &icon_room_przedpokoj, &icon_room_przedpokoj_large},
    {"biuro", "Biuro", "BIURO", &icon_room_biuro, &icon_room_biuro_large},
    {"lazienka", "Łazienka", "ŁAZIENKA", &icon_room_lazienka, &icon_room_lazienka_large},
    {"sypialnia", "Sypialnia", "SYPIALNIA", &icon_room_sypialnia, &icon_room_sypialnia_large},
};

// Klimatyzacja z kontraktu (ADR 0009) wisi w salonie.
constexpr std::size_t kClimateRoom = 0;

// Efekty WiZ: polska nazwa i kolejność na liście. Nieznany efekt pokazujemy tak, jak przyszedł,
// na końcu listy.
struct EffectDef {
    const char* id;
    const char* label;
    const char* icon;   // znak z font_effect_icon() (Lucide), jak w makiecie
};
const EffectDef kEffectNames[] = {
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

// Barwa: od ciepłej do zimnej, stałe kolory poza paletą (nie zmieniają się ze stylem).
const uint32_t kKelvinColors[kKelvinSteps] = {0xffbf75, 0xffd291, 0xffe2b5, 0xfff0da,
                                               0xf6f6f6, 0xe3ecff, 0xcddcff};

constexpr int32_t kStripHeight = 56;
constexpr int32_t kChip = 64;
constexpr int32_t kCardHeight = 124;
constexpr int32_t kRailItem = 64;

// ---------- wspólne kształty ----------

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

lv_obj_t* button(lv_obj_t* parent) {
    lv_obj_t* obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    return obj;
}

lv_obj_t* card(lv_obj_t* parent) {
    lv_obj_t* obj = button(parent);
    lv_obj_add_style(obj, &theme::styles().tile, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, theme::kR4, LV_PART_MAIN);
    return obj;
}

lv_obj_t* dot(lv_obj_t* parent) {
    lv_obj_t* obj = box(parent);
    lv_obj_set_size(obj, 8, 8);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, theme::faint(), LV_PART_MAIN);
    return obj;
}

lv_obj_t* chevron(lv_obj_t* parent) {
    lv_obj_t* label = text_label(parent, theme::font_icon_sm(), theme::faint());
    lv_label_set_text(label, LV_SYMBOL_RIGHT);
    return label;
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

// ---------- dane ----------

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

// ---------- zamiary ----------

void send_power(Context& context, const domain::LightState& light) {
    if (!context.controller->state().lights_fresh() || !light.available) {
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetPower;
    intent.control = domain::ControlId::LightPower;
    intent.flag = !light.on;
    intent.target.assign(light.id.c_str());
    intent.text.assign(light.id.c_str());
    dispatch(context, intent);
}

void on_tile_lamp(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        if (context.home.tiles[i].lamp == target) {
            if (const domain::RoomLight* room = room_light(context.controller->state(), i)) {
                send_power(context, room->main);
            }
            return;
        }
    }
}

void on_room_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    if (const domain::RoomLight* room = room_light(context.controller->state(), context.room_page.room)) {
        send_power(context, room->main);
    }
}

void on_light_power(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    if (const domain::RoomLight* room = room_light(context.controller->state(), context.room_page.room)) {
        if (const domain::LightState* light = current_light(context, *room)) {
            send_power(context, *light);
        }
    }
}

void on_kelvin(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, context.room_page.room);
    if (!state.lights_fresh() || room == nullptr || !room->has_kelvin()) {
        return;
    }
    const domain::LightState* light = current_light(context, *room);
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kKelvinSteps; ++i) {
        if (context.room_page.kelvin[i] == target && light != nullptr) {
            domain::UiIntent intent;
            intent.kind = domain::IntentKind::SetValue;
            intent.control = domain::ControlId::LightKelvin;
            intent.number = static_cast<int16_t>(context.room_page.kelvin_steps[i]);
            intent.target.assign(light->id.c_str());
            intent.text.assign(light->id.c_str());
            dispatch(context, intent);
            return;
        }
    }
}

void on_effect(void* raw) {
    auto& tile = *static_cast<RoomScreen::EffectTile*>(raw);
    Context& context = *tile.context;
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, context.room_page.room);
    if (!state.lights_fresh() || room == nullptr) {
        return;
    }
    // Lista znika od razu; oczekiwanie widać na kaflu efektu.
    context.room_page.effect_picker_open = false;
    widgets::show_page(context.room_page.effect_picker, false);
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetOption;
    intent.control = domain::ControlId::LightEffect;
    intent.target.assign(room->main.id.c_str());
    intent.text.assign(tile.name.c_str());
    intent.payload.assign(tile.name.c_str());
    dispatch(context, intent);
}

// ---------- nawigacja podstrony ----------

void push(Context& context, Level level) {
    RoomScreen& screen = context.room_page;
    if (screen.depth < 3) {
        screen.stack[screen.depth++] = level;
    }
    lv_obj_scroll_to_y(screen.body, 0, LV_ANIM_OFF);
    render(context);
}

void on_back(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    if (context.room_page.depth > 0) {
        --context.room_page.depth;
        render(context);
        return;
    }
    close_room(context);
}

void on_bulb_row(lv_event_t* event) {
    auto& row = *static_cast<RoomScreen::BulbRow*>(lv_event_get_user_data(event));
    row.context->room_page.bulb = row.index;
    push(*row.context, Level::Bulb);
}

void set_drawer(Context& context, int8_t side, bool animate);
void open_effect_picker(Context& context);

void on_rail(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        if (context.room_page.rail_items[i] == target) {
            context.room_page.room = i;
            context.room_page.depth = 0;
            lv_obj_scroll_to_y(context.room_page.body, 0, LV_ANIM_OFF);
            set_drawer(context, 0, true);
            render(context);
            return;
        }
    }
}

void open_climate(lv_event_t* event) {
    open_climate_page(*static_cast<Context*>(lv_event_get_user_data(event)));
}

// ---------- budowa podstrony ----------

RoomScreen::Card make_card(lv_obj_t* parent, bool wide, Context& context) {
    RoomScreen::Card c;
    c.root = card(parent);
    if (wide) {
        // Poziom: dwie wysokie karty obok siebie, studzienka u góry, nazwa u dołu.
        lv_obj_set_height(c.root, LV_PCT(100));
        lv_obj_set_width(c.root, 0);
        lv_obj_set_flex_grow(c.root, 1);
        lv_obj_set_style_pad_all(c.root, theme::kS5, LV_PART_MAIN);
        theme::make_column(c.root, theme::kS1);
    } else {
        lv_obj_set_size(c.root, LV_PCT(100), kCardHeight);
        lv_obj_set_style_pad_hor(c.root, theme::kS4, LV_PART_MAIN);
        theme::make_row(c.root, theme::kS4);
    }
    c.well = box(c.root);
    lv_obj_set_size(c.well, 80, 80);
    lv_obj_set_style_radius(c.well, theme::kR3, LV_PART_MAIN);
    lv_obj_set_style_border_width(c.well, 1, LV_PART_MAIN);
    lv_obj_set_flex_flow(c.well, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c.well, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    paint_glow(c.well, 0);

    if (wide) {
        widgets::make_spacer(c.root);  // nazwa i stan przy dolnej krawędzi karty
    }
    lv_obj_t* texts = box(c.root);
    if (wide) {
        lv_obj_set_size(texts, LV_PCT(100), LV_SIZE_CONTENT);
    } else {
        lv_obj_set_height(texts, LV_SIZE_CONTENT);
        lv_obj_set_width(texts, 0);
        lv_obj_set_flex_grow(texts, 1);
    }
    theme::make_column(texts, 3);
    c.name = text_label(texts, wide ? theme::font_b26() : theme::font_b19(), theme::text());
    c.state = text_label(texts, theme::font_r13(), theme::dim());
    c.note = text_label(texts, theme::font_r11(), theme::faint());
    for (lv_obj_t* label : {c.name, c.state, c.note}) {
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    c.chevron = chevron(c.root);
    if (wide) {
        lv_obj_add_flag(c.chevron, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(c.chevron, LV_ALIGN_BOTTOM_RIGHT, 0, -4);
    }
    (void)context;
    return c;
}

void build_room_view(Context& context, bool wide) {
    RoomScreen& screen = context.room_page;
    screen.view_room = box(screen.body);
    lv_obj_set_width(screen.view_room, LV_PCT(100));
    lv_obj_set_height(screen.view_room, wide ? LV_PCT(100) : LV_SIZE_CONTENT);
    theme::make_column(screen.view_room, theme::kS2);
    widgets::make_cap(screen.view_room, "URZĄDZENIA");
    lv_obj_t* cards = box(screen.view_room);
    lv_obj_set_width(cards, LV_PCT(100));
    if (wide) {
        lv_obj_set_flex_grow(cards, 1);
        theme::make_row(cards, theme::kS3);
    } else {
        lv_obj_set_height(cards, LV_SIZE_CONTENT);
        theme::make_column(cards, theme::kS3);
    }

    screen.lamp = make_card(cards, wide, context);
    screen.lamp.icon = lv_image_create(screen.lamp.well);
    screen.lamp.power = widgets::make_power_button(screen.lamp.root, &on_room_power, &context);
    if (wide) {
        lv_obj_add_flag(screen.lamp.power, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(screen.lamp.power, LV_ALIGN_TOP_RIGHT, 0, 0);
    } else {
        lv_obj_move_to_index(screen.lamp.chevron, -1);  // zasilanie przed strzałką
    }
    lv_obj_add_event_cb(
        screen.lamp.root,
        [](lv_event_t* event) {
            auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
            const domain::RoomLight* room =
                room_light(context.controller->state(), context.room_page.room);
            // Sam przełącznik nie ma czego pokazać dalej — steruje nim zasilanie na karcie.
            if (room != nullptr && (room->dimmable || room->member_count > 0)) {
                push(context, Level::Light);
            }
        },
        LV_EVENT_CLICKED, &context);

    screen.climate = make_card(cards, wide, context);
    screen.climate.icon = text_label(screen.climate.well, theme::font_mode_icon_lg(), theme::accent());
    lv_label_set_text(screen.climate.name, "Klimatyzacja");
    lv_obj_add_event_cb(screen.climate.root, open_climate, LV_EVENT_CLICKED, &context);

    screen.empty = make_card(cards, wide, context);
    lv_obj_set_style_bg_opa(screen.empty.root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(screen.empty.well, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screen.empty.chevron, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(screen.empty.root, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(screen.empty.state, "do wybrania w konfiguracji bridge'a w HA");
}

// Wielkie litery z polskimi znakami — etykiety z HA („Środkowa”) do podpisów kafli.
void upper_pl(char* out, std::size_t size, const char* in) {
    std::size_t o = 0;
    for (std::size_t i = 0; in[i] != '\0' && o + 2 < size; ++i) {
        const auto c = static_cast<unsigned char>(in[i]);
        const auto n = static_cast<unsigned char>(in[i + 1]);
        if (c >= 'a' && c <= 'z') {
            out[o++] = static_cast<char>(c - 32);
        } else if ((c == 0xC4 && (n == 0x85 || n == 0x87 || n == 0x99)) ||
                   (c == 0xC5 && (n == 0x82 || n == 0x84 || n == 0x9B || n == 0xBA || n == 0xBC))) {
            out[o++] = static_cast<char>(c);
            out[o++] = static_cast<char>(n - 1);
            ++i;
        } else if (c == 0xC3 && n == 0xB3) {   // ó → Ó
            out[o++] = static_cast<char>(c);
            out[o++] = static_cast<char>(0x93);
            ++i;
        } else {
            out[o++] = static_cast<char>(c);
        }
    }
    out[o] = '\0';
}

void on_dial(void* raw, int32_t value, bool) {
    auto& context = *static_cast<Context*>(raw);
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, context.room_page.room);
    if (!state.lights_fresh() || room == nullptr) {
        return;
    }
    const domain::LightState* light = current_light(context, *room);
    if (light == nullptr) {
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetValue;
    intent.control = domain::ControlId::LightBrightness;
    intent.value = static_cast<uint8_t>(value < 1 ? 1 : value > 100 ? 100 : value);
    intent.target.assign(light->id.c_str());
    intent.text.assign(light->id.c_str());
    dispatch(context, intent);
}

lv_obj_t* tile_base(lv_obj_t* parent) {
    lv_obj_t* obj = button(parent);
    lv_obj_set_style_radius(obj, theme::kR2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, theme::accent(), LV_PART_MAIN);
    return obj;
}

// Kafel jak „Tryb” na podstronie klimy: wybrany — miękkie bursztynowe tło i obwódka.
void paint_tile(lv_obj_t* obj, bool on) {
    lv_obj_set_style_bg_color(obj, on ? theme::accent() : theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, on ? theme::kAccentSoftOpa : static_cast<lv_opa_t>(LV_OPA_COVER),
                            LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, on ? 1 : 0, LV_PART_MAIN);
}

void build_light_view(Context& context, bool wide) {
    RoomScreen& screen = context.room_page;
    screen.view_light = box(screen.body);
    lv_obj_set_width(screen.view_light, LV_PCT(100));
    lv_obj_set_height(screen.view_light, wide ? LV_PCT(100) : LV_SIZE_CONTENT);
    lv_obj_t* side = screen.view_light;
    if (wide) {
        theme::make_row(screen.view_light, theme::kS5);
        lv_obj_set_flex_align(screen.view_light, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    } else {
        theme::make_column(screen.view_light, theme::kS2);
    }

    // Tarcza jasności: ta sama co temperatura klimy, w procentach. Obrót palcem albo −/+,
    // jedno polecenie na gest (po puszczeniu palca, 0,7 s po ostatnim stuknięciu).
    screen.dial = widgets::make_thermostat(screen.view_light, &on_dial, &context);
    screen.dial->set_percent();
    screen.dial->set_range(5, 100, 5);
    if (wide) {
        lv_obj_set_width(screen.dial->root, 320);
        side = box(screen.view_light);
        lv_obj_set_height(side, LV_SIZE_CONTENT);
        lv_obj_set_width(side, 0);
        lv_obj_set_flex_grow(side, 1);
    } else {
        lv_obj_set_width(screen.dial->root, LV_PCT(100));
        side = box(screen.view_light);
        lv_obj_set_size(side, LV_PCT(100), LV_SIZE_CONTENT);
    }
    theme::make_column(side, theme::kS2);

    // Barwa: siedem próbek od ciepłej do zimnej.
    screen.kelvin_box = box(side);
    lv_obj_set_size(screen.kelvin_box, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_column(screen.kelvin_box, theme::kS2);
    widgets::make_cap(screen.kelvin_box, "BARWA");
    screen.kelvin_row = box(screen.kelvin_box);
    lv_obj_set_size(screen.kelvin_row, LV_PCT(100), 52);
    theme::make_row(screen.kelvin_row, 6);
    for (std::size_t i = 0; i < kKelvinSteps; ++i) {
        lv_obj_t* item = button(screen.kelvin_row);
        lv_obj_set_height(item, LV_PCT(100));
        lv_obj_set_width(item, 0);
        lv_obj_set_flex_grow(item, 1);
        lv_obj_set_style_radius(item, theme::kR2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, lv_color_hex(kKelvinColors[i]), LV_PART_MAIN);
        lv_obj_set_style_border_color(item, theme::accent(), LV_PART_MAIN);
        lv_obj_add_event_cb(item, on_kelvin, LV_EVENT_CLICKED, &context);
        screen.kelvin[i] = item;
    }
    lv_obj_t* scale = box(screen.kelvin_box);
    lv_obj_set_size(scale, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(scale, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scale, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_top(scale, -4, LV_PART_MAIN);
    lv_label_set_text(text_label(scale, theme::font_r11(), theme::faint()), "ciepła");
    screen.kelvin_value = text_label(scale, theme::font_r11(), theme::dim());
    lv_label_set_text(text_label(scale, theme::font_r11(), theme::faint()), "zimna");

    // Grupa: efekt całej lampy i kafle żarówek.
    screen.group_box = box(side);
    lv_obj_set_size(screen.group_box, LV_PCT(100), LV_SIZE_CONTENT);
    theme::make_column(screen.group_box, theme::kS2);
    lv_obj_t* effect_cap = widgets::make_cap(screen.group_box, "EFEKT · CAŁA LAMPA");
    lv_obj_set_style_margin_top(effect_cap, theme::kS1, LV_PART_MAIN);
    screen.effect_row = tile_base(screen.group_box);
    lv_obj_set_size(screen.effect_row, LV_PCT(100), 64);
    lv_obj_set_style_pad_hor(screen.effect_row, theme::kS4, LV_PART_MAIN);
    theme::make_row(screen.effect_row, theme::kS4);
    screen.effect_icon = text_label(screen.effect_row, theme::font_effect_icon(), theme::dim());
    lv_obj_t* effect_texts = box(screen.effect_row);
    lv_obj_set_height(effect_texts, LV_SIZE_CONTENT);
    lv_obj_set_width(effect_texts, 0);
    lv_obj_set_flex_grow(effect_texts, 1);
    theme::make_column(effect_texts, 2);
    widgets::make_cap(effect_texts, "EFEKT");
    screen.effect_value = text_label(effect_texts, theme::font_m15(), theme::text());
    chevron(screen.effect_row);
    lv_obj_add_event_cb(
        screen.effect_row,
        [](lv_event_t* event) { open_effect_picker(*static_cast<Context*>(lv_event_get_user_data(event))); },
        LV_EVENT_CLICKED, &context);

    lv_obj_t* bulbs_cap = widgets::make_cap(screen.group_box, "ŻARÓWKI");
    lv_obj_set_style_margin_top(bulbs_cap, theme::kS1, LV_PART_MAIN);
    lv_obj_t* row = box(screen.group_box);
    lv_obj_set_size(row, LV_PCT(100), wide ? 92 : 88);
    theme::make_row(row, theme::kS2);
    for (std::size_t i = 0; i < domain::kMaxLightMembers; ++i) {
        RoomScreen::BulbRow& tile = screen.bulbs[i];
        tile.context = &context;
        tile.index = i;
        tile.root = tile_base(row);
        lv_obj_set_height(tile.root, LV_PCT(100));
        lv_obj_set_width(tile.root, 0);
        lv_obj_set_flex_grow(tile.root, 1);
        theme::make_column(tile.root, 3);
        lv_obj_set_flex_align(tile.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        tile.icon = lv_image_create(tile.root);
        tile.name = widgets::make_cap(tile.root, "");
        tile.note = text_label(tile.root, theme::font_m15(), theme::text());
        lv_obj_add_event_cb(tile.root, on_bulb_row, LV_EVENT_CLICKED, &tile);
    }
}

void build_effect_picker(Context& context) {
    RoomScreen& screen = context.room_page;
    screen.effect_picker = widgets::make_page(
        "Efekt",
        [](void* raw) {
            auto& context = *static_cast<Context*>(raw);
            context.room_page.effect_picker_open = false;
            widgets::show_page(context.room_page.effect_picker, false);
        },
        &context);
    widgets::dock_sheet(screen.effect_picker);
    for (std::size_t i = 0; i < kEffectTiles; ++i) {
        RoomScreen::EffectTile& tile = screen.effects[i];
        tile.context = &context;
        tile.index = i;
        tile.row = widgets::make_list_row(screen.effect_picker.body, "", "", "", "", &on_effect, &tile);
        // Wiersz wyboru: ikona efektu zamiast symbolu, bez opisu i strzałki.
        tile.icon = lv_obj_get_child(tile.row.root, 0);
        lv_obj_set_style_text_font(tile.icon, theme::font_effect_icon(), LV_PART_MAIN);
        lv_obj_add_flag(lv_obj_get_child(tile.row.root, -1), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tile.row.note, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t* make_rail(lv_obj_t* parent, Context& context, bool wide) {
    lv_obj_t* rail = box(parent);
    RoomScreen& screen = context.room_page;
    if (wide) {
        lv_obj_set_size(rail, theme::kRail, LV_PCT(100));
        lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(rail, theme::surface(), LV_PART_MAIN);
        lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_width(rail, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(rail, theme::line(), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(rail, theme::kS2, LV_PART_MAIN);
        theme::make_column(rail, theme::kS2);
        lv_obj_set_flex_align(rail, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_size(rail, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(rail, theme::kGutter, LV_PART_MAIN);
        lv_obj_set_style_pad_top(rail, theme::kS3, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(rail, 6, LV_PART_MAIN);
        theme::make_row(rail, 6);
    }
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        lv_obj_t* item = button(rail);
        lv_obj_set_height(item, kRailItem);
        if (wide) {
            lv_obj_set_width(item, LV_PCT(100));
        } else {
            lv_obj_set_width(item, 0);
            lv_obj_set_flex_grow(item, 1);
        }
        lv_obj_set_style_radius(item, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(item, theme::accent(), LV_PART_MAIN);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* icon = lv_image_create(item);
        lv_image_set_src(icon, kRooms[i].icon);
        // Znacznik wybranego pokoju: kreska przy krawędzi, jak przy zakładce.
        lv_obj_set_style_border_color(item, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_border_side(item, wide ? LV_BORDER_SIDE_RIGHT : LV_BORDER_SIDE_BOTTOM,
                                     LV_PART_MAIN);
        lv_obj_add_event_cb(item, on_rail, LV_EVENT_CLICKED, &context);
        screen.rail_items[i] = item;
    }
    return rail;
}

// ---------- poziom: szuflady przy krawędziach ----------

constexpr int32_t kEdgeZone = 28;   // pas chwytu gestu przy krawędzi
constexpr int32_t kSwipe = 36;      // tyle trzeba przeciągnąć w głąb ekranu
constexpr uint32_t kDrawerMs = 180;

int32_t screen_width() {
    return lv_display_get_horizontal_resolution(lv_display_get_default());
}

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

void set_drawer(Context& context, int8_t side, bool animate) {
    RoomScreen& screen = context.room_page;
    if (screen.drawer_tabs == nullptr) {
        return;
    }
    screen.drawer = side;
    screen.swipe = 0;
    const int32_t width = screen_width();
    slide(screen.drawer_tabs, side == -1 ? 0 : -theme::kRail - 12, animate);
    slide(screen.drawer_rooms, side == 1 ? width - theme::kRail : width + 12, animate);
    if (side != 0) {
        lv_obj_remove_flag(screen.scrim, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen.scrim, LV_OBJ_FLAG_HIDDEN);
    }
}

// Pas przy krawędzi: przeciągnięcie w głąb ekranu wysuwa szufladę, samo dotknięcie też
// (krawędź panelu bywa mniej czuła, a stuknięcie w uchwyt jest oczywiste).
void on_edge(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    RoomScreen& screen = context.room_page;
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    const int8_t side = target == screen.edges[0] ? -1 : 1;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            screen.swipe = side;
            screen.swipe_x = touch_x();
            break;
        case LV_EVENT_PRESSING:
            if (screen.swipe == side && side * (screen.swipe_x - touch_x()) <= -kSwipe) {
                set_drawer(context, side, true);
            }
            break;
        case LV_EVENT_CLICKED:
            if (screen.drawer == 0) {
                set_drawer(context, side, true);
            }
            break;
        default:
            break;
    }
}

// Przyciemnienie: dotknięcie chowa szufladę, przeciągnięcie w stronę jej krawędzi też.
void on_scrim(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    RoomScreen& screen = context.room_page;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            screen.swipe_x = touch_x();
            break;
        case LV_EVENT_PRESSING:
            if (screen.drawer != 0 && screen.drawer * (touch_x() - screen.swipe_x) >= kSwipe) {
                set_drawer(context, 0, true);
            }
            break;
        case LV_EVENT_CLICKED:
            set_drawer(context, 0, true);
            break;
        default:
            break;
    }
}

void on_drawer_tab(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    const auto index = static_cast<std::size_t>(lv_obj_get_index(target));
    if (index < kScreenCount) {
        close_room(context);
        show_screen(context, static_cast<ScreenId>(index));
    }
}

void build_drawers(Context& context) {
    RoomScreen& screen = context.room_page;
    const int32_t width = screen_width();

    for (int i = 0; i < 2; ++i) {
        lv_obj_t* edge = button(screen.root);
        lv_obj_add_flag(edge, LV_OBJ_FLAG_FLOATING);
        lv_obj_remove_flag(edge, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_set_size(edge, kEdgeZone, LV_PCT(100));
        lv_obj_set_pos(edge, i == 0 ? 0 : width - kEdgeZone, 0);
        // Uchwyt: podpowiedź, że przy krawędzi coś jest.
        lv_obj_t* handle = box(edge);
        lv_obj_set_size(handle, 5, 64);
        lv_obj_set_style_radius(handle, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(handle, theme::surface3(), LV_PART_MAIN);
        lv_obj_align(handle, i == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, i == 0 ? 5 : -5, 0);
        for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_CLICKED}) {
            lv_obj_add_event_cb(edge, on_edge, code, &context);
        }
        screen.edges[i] = edge;
    }

    screen.scrim = button(screen.root);
    lv_obj_add_flag(screen.scrim, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(screen.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(screen.scrim, 0, 0);
    lv_obj_set_style_bg_opa(screen.scrim, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.scrim, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_flag(screen.scrim, LV_OBJ_FLAG_HIDDEN);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_CLICKED}) {
        lv_obj_add_event_cb(screen.scrim, on_scrim, code, &context);
    }

    // Lewa szuflada: zakładki jak w pasku powłoki; Dom zaznaczony.
    static const lv_image_dsc_t* const kTabIcons[kScreenCount] = {
        &icon_tv, &icon_appletv, &icon_audio, &icon_home, &icon_settings};
    screen.drawer_tabs = box(screen.root);
    lv_obj_add_flag(screen.drawer_tabs, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.drawer_tabs, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(screen.drawer_tabs, theme::kRail, LV_PCT(100));
    lv_obj_set_style_bg_opa(screen.drawer_tabs, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.drawer_tabs, theme::surface(), LV_PART_MAIN);
    lv_obj_set_style_border_side(screen.drawer_tabs, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen.drawer_tabs, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(screen.drawer_tabs, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(screen.drawer_tabs, theme::kS2, LV_PART_MAIN);
    theme::make_column(screen.drawer_tabs, theme::kS2);
    lv_obj_set_flex_align(screen.drawer_tabs, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    for (std::size_t i = 0; i < kScreenCount; ++i) {
        lv_obj_t* tab = button(screen.drawer_tabs);
        lv_obj_set_size(tab, LV_PCT(100), theme::kRailTab);
        lv_obj_set_style_radius(tab, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tab, theme::accent(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tab, i == static_cast<std::size_t>(ScreenId::Home)
                                         ? theme::kAccentSoftOpa
                                         : static_cast<lv_opa_t>(LV_OPA_TRANSP),
                                LV_PART_MAIN);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* icon = lv_image_create(tab);
        lv_image_set_src(icon, kTabIcons[i]);
        lv_obj_add_event_cb(tab, on_drawer_tab, LV_EVENT_CLICKED, &context);
    }

    // Prawa szuflada: pasek pomieszczeń.
    screen.drawer_rooms = make_rail(screen.root, context, true);
    lv_obj_add_flag(screen.drawer_rooms, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.drawer_rooms, LV_OBJ_FLAG_CLICKABLE);

    for (lv_obj_t* drawer : {screen.drawer_tabs, screen.drawer_rooms}) {
        lv_obj_set_y(drawer, 0);
        lv_obj_set_style_shadow_width(drawer, 30, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(drawer, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(drawer, lv_color_black(), LV_PART_MAIN);
    }
    set_drawer(context, 0, false);
}

void build_room_page(Context& context) {
    RoomScreen& screen = context.room_page;
    const bool wide = theme::landscape();

    // Pełny ekran na warstwie systemowej, jak każda podstrona. W poziomie paski są
    // schowane w szufladach (build_drawers), więc treść ma całą szerokość.
    screen.root = lv_obj_create(lv_layer_sys());
    theme::make_flat(screen.root);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(screen.root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen.root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(screen.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.root, theme::bg(), LV_PART_MAIN);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* column = screen.root;
    theme::make_column(column, 0);

    // W poziomie margines szerszy o pas chwytu gestu przy krawędzi.
    const int32_t gutter = wide ? theme::kGutterWide + 8 : theme::kGutter;
    lv_obj_t* bar = box(column);
    lv_obj_set_size(bar, LV_PCT(100), theme::kTopbar);
    lv_obj_set_style_pad_hor(bar, gutter, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, theme::kTopbarPad, LV_PART_MAIN);
    theme::make_row(bar, theme::kS3);
    lv_obj_t* back = button(bar);
    lv_obj_set_size(back, 56, 56);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back, theme::surface(), LV_PART_MAIN);
    lv_obj_set_flex_flow(back, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(back, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* arrow = text_label(back, theme::font_icon(), theme::dim());
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, &context);
    lv_obj_t* titles = box(bar);
    lv_obj_set_height(titles, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(titles, 1);
    theme::make_column(titles, 2);
    screen.title = text_label(titles, theme::font_b19(), theme::text());
    screen.sub = widgets::make_cap(titles, "");
    // Zasilanie w nagłówku po prawej, jak na każdym ekranie jednego urządzenia. Widoczne
    // na poziomach światła (grupa, żarówka, efekty); pokój ma kilka urządzeń.
    screen.light_power = widgets::make_power_button(bar, &on_light_power, &context);

    if (!wide) {
        lv_obj_t* rail = make_rail(column, context, false);
        // Linia pod paskiem pomieszczeń, wcięta jak treść.
        lv_obj_t* rule = box(column);
        lv_obj_set_size(rule, LV_PCT(100), 1);
        lv_obj_set_style_margin_hor(rule, theme::kGutter, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(rule, theme::surface2(), LV_PART_MAIN);
        lv_obj_set_width(rule, theme::kContentW);
        (void)rail;
    }

    screen.body = box(column);
    lv_obj_set_width(screen.body, LV_PCT(100));
    lv_obj_set_flex_grow(screen.body, 1);
    lv_obj_set_style_pad_hor(screen.body, gutter, LV_PART_MAIN);
    lv_obj_set_style_pad_top(screen.body, wide ? 0 : theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(screen.body, theme::kS4, LV_PART_MAIN);
    theme::make_column(screen.body, 0);
    lv_obj_add_flag(screen.body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(screen.body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(screen.body, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_width(screen.body, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(screen.body, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(screen.body, theme::faint(), LV_PART_SCROLLBAR);

    build_room_view(context, wide);
    build_light_view(context, wide);
    build_effect_picker(context);

    if (wide) {
        build_drawers(context);
    }
    screen.built = true;
}

void open_room(Context& context, std::size_t index) {
    RoomScreen& screen = context.room_page;
    if (!screen.built) {
        build_room_page(context);
    }
    screen.room = index;
    screen.depth = 0;
    screen.open = true;
    set_drawer(context, 0, false);
    lv_obj_remove_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(screen.root);
    lv_obj_scroll_to_y(screen.body, 0, LV_ANIM_OFF);
    render(context);
}

void on_tile(lv_event_t* event) {
    auto& context = *static_cast<Context*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        if (context.home.tiles[i].root == target) {
            open_room(context, i);
            return;
        }
    }
}

// ---------- budowa ekranu Dom ----------

void build_climate_entry(Context& context, lv_obj_t* parent, bool wide) {
    HomeScreen& screen = context.home;
    screen.climate = card(parent);
    if (wide) {
        lv_obj_set_size(screen.climate, LV_SIZE_CONTENT, 46);
        lv_obj_set_style_radius(screen.climate, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_pad_left(screen.climate, theme::kS4, LV_PART_MAIN);
        lv_obj_set_style_pad_right(screen.climate, theme::kS3, LV_PART_MAIN);
    } else {
        lv_obj_set_size(screen.climate, LV_PCT(100), kStripHeight);
        lv_obj_set_style_radius(screen.climate, theme::kR3, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(screen.climate, theme::kS4, LV_PART_MAIN);
    }
    theme::make_row(screen.climate, theme::kS3);
    screen.climate_dot = dot(screen.climate);
    lv_obj_t* name = text_label(screen.climate, theme::font_m15(), theme::text());
    lv_label_set_text(name, "Klimatyzacja");
    screen.climate_text = text_label(screen.climate, theme::font_r13(), theme::dim());
    if (!wide) {
        lv_obj_set_flex_grow(screen.climate_text, 1);
    }
    chevron(screen.climate);
    lv_obj_add_event_cb(screen.climate, open_climate, LV_EVENT_CLICKED, &context);
}

void build_tile(Context& context, lv_obj_t* grid, std::size_t index, int32_t col, int32_t row) {
    HomeScreen::RoomTile& tile = context.home.tiles[index];
    tile.root = card(grid);
    lv_obj_set_grid_cell(tile.root, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_pad_top(tile.root, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(tile.root, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(tile.root, theme::kS4, LV_PART_MAIN);
    theme::make_column(tile.root, 0);

    lv_obj_t* icon = lv_image_create(tile.root);
    lv_image_set_src(icon, kRooms[index].large);
    lv_obj_set_style_margin_left(icon, -6, LV_PART_MAIN);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* spacer = widgets::make_spacer(tile.root);
    (void)spacer;
    lv_obj_t* name = text_label(tile.root, theme::font_b26(), theme::text());
    lv_label_set_text(name, kRooms[index].name);
    lv_obj_t* row_box = box(tile.root);
    lv_obj_set_size(row_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_margin_top(row_box, theme::kS1, LV_PART_MAIN);
    theme::make_row(row_box, 7);
    tile.dot = dot(row_box);
    tile.state = text_label(row_box, theme::font_r13(), theme::dim());
    lv_obj_set_width(tile.state, 0);
    lv_obj_set_flex_grow(tile.state, 1);
    lv_label_set_long_mode(tile.state, LV_LABEL_LONG_DOT);

    // Lampka: osobny przycisk w rogu, poza układem kafelka.
    tile.lamp = button(tile.root);
    lv_obj_add_flag(tile.lamp, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(tile.lamp, kChip, kChip);
    lv_obj_align(tile.lamp, LV_ALIGN_TOP_RIGHT, 4, 2);
    lv_obj_set_ext_click_area(tile.lamp, 6);
    lv_obj_set_style_radius(tile.lamp, theme::kR4 - 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile.lamp, 1, LV_PART_MAIN);
    paint_glow(tile.lamp, 0);
    tile.lamp_icon = lv_image_create(tile.lamp);
    lv_obj_align(tile.lamp_icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(tile.lamp_icon, LV_OBJ_FLAG_CLICKABLE);
    tile.lamp_bar = box(tile.lamp);
    lv_obj_set_size(tile.lamp_bar, kChip - 22, 3);
    lv_obj_align(tile.lamp_bar, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_radius(tile.lamp_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile.lamp_bar, 90, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tile.lamp_bar, lv_color_black(), LV_PART_MAIN);
    tile.lamp_fill = box(tile.lamp_bar);
    lv_obj_set_height(tile.lamp_fill, LV_PCT(100));
    lv_obj_set_style_radius(tile.lamp_fill, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile.lamp_fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tile.lamp_fill, theme::accent(), LV_PART_MAIN);
    lv_obj_add_event_cb(tile.lamp, on_tile_lamp, LV_EVENT_CLICKED, &context);

    lv_obj_add_event_cb(tile.root, on_tile, LV_EVENT_CLICKED, &context);
}

// ---------- rysowanie ----------

void light_summary(char* out, std::size_t size, const domain::RoomLight& room,
                   const domain::LightState& light) {
    if (!light.available) {
        std::snprintf(out, size, "%s · niedostępne", light.label.c_str());
    } else if (!light.on) {
        std::snprintf(out, size, "%s · wył.", light.label.c_str());
    } else if (&light == &room.main && !room.effect.empty()) {
        std::snprintf(out, size, "%s · %s", light.label.c_str(), effect_label(room.effect.c_str()));
    } else if (room.dimmable) {
        std::snprintf(out, size, "%s · %u%%", light.label.c_str(),
                      static_cast<unsigned>(light.brightness));
    } else {
        std::snprintf(out, size, "%s · wł.", light.label.c_str());
    }
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

void render_tiles(Context& context) {
    HomeScreen& screen = context.home;
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.lights_fresh();

    if (screen.climate != nullptr) {
        char text[48];
        climate_summary(text, sizeof(text), state);
        lv_label_set_text(screen.climate_text, text);
        const bool on = state.climate_fresh() && state.snapshot().climate.is_on();
        lv_obj_set_style_bg_color(screen.climate_dot,
                                  !state.climate_fresh() ? theme::danger()
                                  : on                   ? theme::accent()
                                                         : theme::faint(),
                                  LV_PART_MAIN);
    }

    for (std::size_t i = 0; i < kRoomCount; ++i) {
        HomeScreen::RoomTile& tile = screen.tiles[i];
        if (tile.root == nullptr) {
            continue;
        }
        const domain::RoomLight* room = room_light(state, i);
        char text[64];
        lv_color_t dot_color = theme::surface3();
        int glow = 0;
        if (room == nullptr) {
            // Bez sekcji `lights` nie wiemy, czy pokój ma światło — nie udajemy, że nie ma.
            std::snprintf(text, sizeof(text), "%s", fresh ? "brak światła" : "czekam na stan");
            lv_obj_set_style_opa(tile.lamp, 90, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(tile.lamp, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(tile.lamp, theme::line(), LV_PART_MAIN);
            lv_obj_add_flag(tile.lamp_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(tile.lamp_bar, LV_OBJ_FLAG_HIDDEN);
            widgets::set_status_outline(tile.lamp, domain::ControlStatus::Idle);
        } else {
            const domain::LightState& light = room->main;
            glow = fresh && light.available ? glow_of(*room, light) : 0;
            if (fresh) {
                light_summary(text, sizeof(text), *room, light);
                dot_color = !light.available ? theme::danger() : light.on ? theme::accent()
                                                                          : theme::faint();
            } else {
                std::snprintf(text, sizeof(text), "brak świeżego stanu");
                dot_color = theme::danger();
            }
            lv_obj_set_style_opa(tile.lamp, fresh ? LV_OPA_COVER : 128, LV_PART_MAIN);
            paint_glow(tile.lamp, glow);
            lv_obj_remove_flag(tile.lamp_icon, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(tile.lamp_icon, lamp_icon(fresh, light, glow));
            if (glow > 0 && room->dimmable) {
                lv_obj_remove_flag(tile.lamp_bar, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_width(tile.lamp_fill, LV_PCT(light.brightness));
            } else {
                lv_obj_add_flag(tile.lamp_bar, LV_OBJ_FLAG_HIDDEN);
            }
            widgets::set_status_outline(tile.lamp,
                                        status_for(state, domain::ControlId::LightPower,
                                                   light.id.c_str()));
        }
        lv_label_set_text(tile.state, text);
        lv_obj_set_style_bg_color(tile.dot, dot_color, LV_PART_MAIN);
        // Zapalone światło: ciepła obwódka całego kafelka.
        lv_obj_set_style_border_color(tile.root, glow > 0 ? theme::accent() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(tile.root, glow > 0 ? 110 : LV_OPA_COVER, LV_PART_MAIN);
    }
}

void show(lv_obj_t* obj, bool visible) {
    if (visible) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void render_room_view(Context& context, const domain::RoomLight* room) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.lights_fresh();
    const bool climate_here = screen.room == kClimateRoom;

    show(screen.lamp.root, room != nullptr);
    show(screen.climate.root, climate_here);
    show(screen.empty.root, room == nullptr);
    // Bez sekcji `lights` (stary bridge, brak stanu) nie wiemy, czy pokój ma światło.
    if (!fresh) {
        lv_label_set_text(screen.empty.name, "Oświetlenie");
        lv_label_set_text(screen.empty.state, "czekam na stan z Home Assistanta");
    } else {
        lv_label_set_text(screen.empty.name, climate_here ? "Inne encje" : "Brak encji");
        lv_label_set_text(screen.empty.state, "do wybrania w konfiguracji bridge'a w HA");
    }

    if (room != nullptr) {
        const domain::LightState& light = room->main;
        const int glow = fresh && light.available ? glow_of(*room, light) : 0;
        lv_label_set_text(screen.lamp.name, light.label.c_str());
        lv_image_set_src(screen.lamp.icon, lamp_icon(fresh, light, glow));
        paint_glow(screen.lamp.well, glow);
        char text[64];
        if (!fresh) {
            std::snprintf(text, sizeof(text), "brak świeżego stanu");
        } else if (!light.available) {
            std::snprintf(text, sizeof(text), "niedostępna");
        } else if (!light.on) {
            std::snprintf(text, sizeof(text), "wyłączona");
        } else if (!room->effect.empty()) {
            std::snprintf(text, sizeof(text), "efekt: %s", effect_label(room->effect.c_str()));
        } else if (room->dimmable && light.kelvin > 0) {
            std::snprintf(text, sizeof(text), "%u%% · %u K", static_cast<unsigned>(light.brightness),
                          static_cast<unsigned>(light.kelvin));
        } else if (room->dimmable) {
            std::snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(light.brightness));
        } else {
            std::snprintf(text, sizeof(text), "włączona");
        }
        lv_label_set_text(screen.lamp.state, text);
        if (room->member_count > 0) {
            std::size_t lit = 0;
            for (std::size_t i = 0; i < room->member_count; ++i) {
                lit += room->members[i].on ? 1 : 0;
            }
            std::snprintf(text, sizeof(text), "%u z %u żarówek", static_cast<unsigned>(lit),
                          static_cast<unsigned>(room->member_count));
        } else {
            text[0] = '\0';
        }
        lv_label_set_text(screen.lamp.note, text);
        show(screen.lamp.chevron, room->dimmable || room->member_count > 0);
        widgets::set_power_state(screen.lamp.power, fresh && light.on, fresh && light.available);
        widgets::set_status_outline(screen.lamp.power,
                                    status_for(state, domain::ControlId::LightPower, light.id.c_str()));
    }

    if (climate_here) {
        char text[48];
        climate_summary(text, sizeof(text), state);
        lv_label_set_text(screen.climate.state, text);
        const domain::ClimateState& climate = state.snapshot().climate;
        if (state.climate_fresh() && climate.has_current) {
            char temp[16];
            widgets::format_tenths(temp, sizeof(temp), climate.current_tenths);
            lv_label_set_text_fmt(screen.climate.note, "w pokoju %s°", temp);
        } else {
            lv_label_set_text(screen.climate.note, "");
        }
        lv_label_set_text(screen.climate.icon, theme::kIconCool);
        const bool on = state.climate_fresh() && climate.is_on();
        paint_glow(screen.climate.well, on ? 1 : 0);
    }
}

const char* kelvin_name(std::size_t step) {
    static const char* const kNames[kKelvinSteps] = {"bardzo ciepła", "ciepła", "ciepła biel",
                                                     "neutralna", "biała", "chłodna", "zimna"};
    return step < kKelvinSteps ? kNames[step] : "";
}

void render_light_view(Context& context, const domain::RoomLight& room, bool group) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const bool fresh = state.lights_fresh();
    const domain::LightState* light_ptr = current_light(context, room);
    if (light_ptr == nullptr) {
        return;
    }
    const domain::LightState& light = *light_ptr;
    const bool usable = fresh && light.available;
    const bool on = usable && light.on;
    const bool effect_on = group && on && !room.effect.empty();

    // Barwa: siedem kroków równo w zakresie encji, zaznaczony najbliższy bieżącej.
    const domain::PendingEntry* kpending = state.pending_of(domain::ControlId::LightKelvin);
    const bool kmine = kpending != nullptr && kpending->requested == light.id.c_str();
    const int32_t current_k = kmine ? kpending->requested_number : light.kelvin;
    std::size_t nearest = kKelvinSteps;
    if (room.has_kelvin()) {
        int32_t best = 1 << 20;
        const int32_t span = room.max_kelvin - room.min_kelvin;
        for (std::size_t i = 0; i < kKelvinSteps; ++i) {
            const int32_t kelvin =
                (room.min_kelvin + span * static_cast<int32_t>(i) / (kKelvinSteps - 1) + 50) / 100 * 100;
            screen.kelvin_steps[i] = static_cast<uint16_t>(kelvin);
            const int32_t distance = current_k > kelvin ? current_k - kelvin : kelvin - current_k;
            if (current_k > 0 && distance < best) {
                best = distance;
                nearest = i;
            }
        }
    }

    // Tarcza: wartość w toku, potem potwierdzona. Pod palcem należy do tarczy.
    widgets::Thermostat& dial = *screen.dial;
    dial.set_enabled(on && room.dimmable);
    const domain::PendingEntry* bpending = state.pending_of(domain::ControlId::LightBrightness);
    const bool bmine = bpending != nullptr && bpending->requested == light.id.c_str();
    if (!dial.busy()) {
        dial.set_value(bmine ? bpending->requested_value : light.brightness);
        dial.set_status(bmine ? state.status_of(domain::ControlId::LightBrightness)
                              : domain::ControlStatus::Idle);
    }
    char note[48] = "";
    if (effect_on) {
        std::snprintf(note, sizeof(note), "%s", effect_label(room.effect.c_str()));
    } else if (on && nearest < kKelvinSteps && current_k > 0) {
        std::snprintf(note, sizeof(note), "%u K · %s", static_cast<unsigned>(current_k),
                      kelvin_name(nearest));
    } else if (!fresh) {
        std::snprintf(note, sizeof(note), "brak świeżego stanu");
    } else if (!light.available) {
        std::snprintf(note, sizeof(note), "niedostępna");
    }
    dial.set_texts(!on ? "WYŁĄCZONA" : effect_on ? "EFEKT" : "JASNOŚĆ", note);
    if (!on) {
        lv_label_set_text(dial.value_label, "—");
        lv_obj_add_flag(dial.unit_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(dial.unit_label, LV_OBJ_FLAG_HIDDEN);
    }

    show(screen.kelvin_box, room.has_kelvin());
    if (room.has_kelvin()) {
        const bool active = on && !effect_on;
        for (std::size_t i = 0; i < kKelvinSteps; ++i) {
            const bool selected = active && i == nearest;
            lv_obj_set_style_bg_opa(screen.kelvin[i], !active ? 50 : selected ? LV_OPA_COVER : 140,
                                    LV_PART_MAIN);
            // Niewybrana próbka ma cienką obwódkę linii: w jasnym stylu najbielsza ginęła na tle.
            lv_obj_set_style_border_width(screen.kelvin[i], selected ? 3 : 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(screen.kelvin[i], selected ? theme::accent() : theme::line(),
                                          LV_PART_MAIN);
        }
        widgets::set_status_outline(screen.kelvin_row,
                                    kmine ? state.status_of(domain::ControlId::LightKelvin)
                                          : domain::ControlStatus::Idle);
        lv_label_set_text(screen.kelvin_value, effect_on ? "barwa nie działa w efekcie" : "");
    }

    widgets::set_power_state(screen.light_power, on, usable);
    widgets::set_status_outline(screen.light_power,
                                status_for(state, domain::ControlId::LightPower, light.id.c_str()));

    // Efekt i żarówki: tylko grupa.
    show(screen.group_box, group && (room.member_count > 0 || state.snapshot().lights.effect_count > 0));
    if (!group) {
        return;
    }
    const domain::PendingEntry* effect_pending = state.pending_of(domain::ControlId::LightEffect);
    const char* effect = effect_pending != nullptr ? effect_pending->requested.c_str() : room.effect.c_str();
    lv_label_set_text(screen.effect_value, effect[0] != '\0' ? effect_label(effect) : "Brak");
    lv_label_set_text(screen.effect_icon, effect_icon(effect));
    paint_tile(screen.effect_row, effect_on);
    lv_obj_set_style_text_color(screen.effect_icon, effect_on ? theme::accent() : theme::dim(), LV_PART_MAIN);
    lv_obj_set_style_text_color(screen.effect_value, effect_on ? theme::accent() : theme::text(), LV_PART_MAIN);
    widgets::set_status_outline(screen.effect_row, state.status_of(domain::ControlId::LightEffect));
    show(screen.effect_row, state.snapshot().lights.effect_count > 0);

    for (std::size_t i = 0; i < domain::kMaxLightMembers; ++i) {
        RoomScreen::BulbRow& tile = screen.bulbs[i];
        if (i >= room.member_count) {
            show(tile.root, false);
            continue;
        }
        show(tile.root, true);
        const domain::LightState& bulb = room.members[i];
        const bool bulb_on = fresh && bulb.available && bulb.on;
        lv_image_set_src(tile.icon, bulb_on ? &icon_bulb_on : &icon_bulb_off);
        char caps[32];
        upper_pl(caps, sizeof(caps), bulb.label.c_str());
        lv_label_set_text(tile.name, caps);
        if (!fresh || !bulb.available) {
            lv_label_set_text(tile.note, "—");
        } else if (!bulb.on) {
            lv_label_set_text(tile.note, "wył.");
        } else {
            lv_label_set_text_fmt(tile.note, "%u%%", static_cast<unsigned>(bulb.brightness));
        }
        paint_tile(tile.root, bulb_on);
        lv_obj_set_style_text_color(tile.name, bulb_on ? theme::accent() : theme::faint(), LV_PART_MAIN);
        lv_obj_set_style_text_color(tile.note, bulb_on ? theme::accent() : theme::text(), LV_PART_MAIN);
        widgets::set_status_outline(tile.root,
                                    status_for(state, domain::ControlId::LightPower, bulb.id.c_str()));
    }
}

void render_effect_picker(Context& context, const domain::RoomLight& room) {
    RoomScreen& screen = context.room_page;
    const domain::AppState& state = context.controller->state();
    const domain::LightsState& lights = state.snapshot().lights;

    // Kolejność: znane efekty w kolejności z tabeli, potem nieznane z listy.
    std::size_t slot = 0;
    auto place = [&](const char* name) {
        if (slot >= kEffectTiles) {
            return;
        }
        RoomScreen::EffectTile& tile = screen.effects[slot++];
        tile.name.assign(name);
        lv_label_set_text(tile.row.title, name[0] != '\0' ? effect_label(name) : "Brak");
        lv_label_set_text(tile.icon, effect_icon(name));
        const bool current = state.lights_fresh() && room.effect == name;
        lv_label_set_text(tile.row.value, current ? "aktywny" : "");
        lv_obj_set_style_border_color(tile.row.root, current ? theme::accent() : theme::line(),
                                      LV_PART_MAIN);
        lv_obj_set_style_text_color(tile.icon, current ? theme::accent() : theme::dim(), LV_PART_MAIN);
        show(tile.row.root, true);
    };
    place("");
    for (const EffectDef& def : kEffectNames) {
        for (std::size_t i = 0; i < lights.effect_count; ++i) {
            if (lights.effects[i] == def.id) {
                place(def.id);
                break;
            }
        }
    }
    for (std::size_t i = 0; i < lights.effect_count; ++i) {
        bool known = false;
        for (const EffectDef& def : kEffectNames) {
            known = known || lights.effects[i] == def.id;
        }
        if (!known) {
            place(lights.effects[i].c_str());
        }
    }
    for (std::size_t i = slot; i < kEffectTiles; ++i) {
        show(screen.effects[i].row.root, false);
    }
}

void open_effect_picker(Context& context) {
    RoomScreen& screen = context.room_page;
    const domain::RoomLight* room = room_light(context.controller->state(), screen.room);
    if (room == nullptr || !context.controller->state().lights_fresh()) {
        return;
    }
    screen.effect_picker_open = true;
    render_effect_picker(context, *room);
    lv_obj_scroll_to_y(screen.effect_picker.body, 0, LV_ANIM_OFF);
    widgets::show_page(screen.effect_picker, true);
}

void render_room(Context& context) {
    RoomScreen& screen = context.room_page;
    if (!screen.built || !screen.open) {
        return;
    }
    const domain::AppState& state = context.controller->state();
    const domain::RoomLight* room = room_light(state, screen.room);
    // Poziom głębszy niż pokój bez światła (np. bridge przestał je podawać) — wracamy.
    if (room == nullptr) {
        screen.depth = 0;
    }
    const Level level = screen.depth > 0 ? screen.stack[screen.depth - 1] : Level::Room;
    if (level == Level::Bulb && (room == nullptr || screen.bulb >= room->member_count)) {
        screen.depth = 0;
    }
    const Level shown = screen.depth > 0 ? screen.stack[screen.depth - 1] : Level::Room;

    for (std::size_t i = 0; i < kRoomCount; ++i) {
        const bool selected = i == screen.room;
        lv_obj_t* item = screen.rail_items[i];
        lv_obj_set_style_bg_opa(item, selected ? theme::kAccentSoftOpa : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(item, selected ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_image_opa(lv_obj_get_child(item, 0), selected ? LV_OPA_COVER : 150, LV_PART_MAIN);
    }

    show(screen.light_power, shown != Level::Room);
    show(screen.view_room, shown == Level::Room);
    show(screen.view_light, shown == Level::Light || shown == Level::Bulb);
    if (screen.effect_picker_open && room != nullptr) {
        render_effect_picker(context, *room);
    }

    char sub[64];
    if (shown == Level::Room) {
        lv_label_set_text(screen.title, kRooms[screen.room].name);
        lv_label_set_text(screen.sub, "POMIESZCZENIE");
        render_room_view(context, room);
        return;
    }
    const bool group = shown == Level::Light;
    if (group) {
        lv_label_set_text(screen.title, room->main.label.c_str());
        if (room->member_count > 0) {
            std::snprintf(sub, sizeof(sub), "%s · %u ŻARÓWKI", kRooms[screen.room].caps,
                          static_cast<unsigned>(room->member_count));
        } else {
            std::snprintf(sub, sizeof(sub), "%s", kRooms[screen.room].caps);
        }
    } else {
        std::snprintf(sub, sizeof(sub), "Żarówka %s", room->members[screen.bulb].label.c_str());
        lv_label_set_text(screen.title, sub);
        char caps[32];
        upper_pl(caps, sizeof(caps), room->main.label.c_str());
        std::snprintf(sub, sizeof(sub), "%s · %s", caps, kRooms[screen.room].caps);
    }
    lv_label_set_text(screen.sub, sub);
    render_light_view(context, *room, group);
}

}  // namespace

void build_home(Context& context, lv_obj_t* parent) {
    HomeScreen& screen = context.home;
    screen.section = widgets::make_section(parent);
    const bool wide = theme::landscape();

    if (wide) {
        // Poziom: klimatyzacja jako pigułka w nagłówku powłoki, pomieszczenia 3 × 2.
        lv_obj_t* slot = make_header_slot(context, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(slot, 1);
        lv_obj_set_flex_align(slot, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        build_climate_entry(context, slot, true);
    } else {
        build_climate_entry(context, screen.section, false);
    }

    static const int32_t kCols2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t kCols3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
    static const int32_t kRows2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t kRows3[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
    lv_obj_t* grid = box(screen.section);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, wide ? kCols3 : kCols2, wide ? kRows2 : kRows3);
    lv_obj_set_style_pad_column(grid, theme::kS3, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, theme::kS3, LV_PART_MAIN);
    const int32_t columns = wide ? 3 : 2;
    for (std::size_t i = 0; i < kRoomCount; ++i) {
        build_tile(context, grid, i, static_cast<int32_t>(i) % columns,
                   static_cast<int32_t>(i) / columns);
    }
}

void render_home(Context& context) {
    render_climate(context);
    HomeScreen& screen = context.home;
    if (screen.climate != nullptr && theme::landscape()) {
        show_header_slot(context, lv_obj_get_parent(screen.climate), ScreenId::Home, true);
    }
    render_tiles(context);
    render_room(context);
}

void close_room(Context& context) {
    RoomScreen& screen = context.room_page;
    if (!screen.built) {
        return;
    }
    screen.open = false;
    screen.depth = 0;
    screen.effect_picker_open = false;
    widgets::show_page(screen.effect_picker, false);
    set_drawer(context, 0, false);
    lv_obj_add_flag(screen.root, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace ui
