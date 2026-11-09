#pragma once

// Obraz stanu urządzeń tak, jak opisuje go kontrakt MQTT v1
// (docs/architecture/mqtt-contract.md, temat `state`).
//
// Składa go `application::LiveSource` z klientów transportu: każdy dostarcza
// wyłącznie sekcję swojego urządzenia.

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/text.hpp"

namespace domain {

// Pilot nigdy nie zgaduje stanu zasilania: brak świeżej informacji to `Unknown`.
enum class Power : uint8_t { Unknown, Off, On };

using Label = core::Text<28>;

inline constexpr std::size_t kMaxOptions = 8;

// Lista opcji przychodzi w snapshocie; firmware nie zaszywa nazw na stałe.
class OptionList {
public:
    void clear() { count_ = 0; }

    void add(const char* value) {
        if (count_ >= kMaxOptions) {
            return;
        }
        items_[count_].assign(value);
        ++count_;
    }

    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    const Label& operator[](std::size_t index) const { return items_[index]; }

private:
    std::array<Label, kMaxOptions> items_{};
    std::size_t count_ = 0;
};

struct TvState {
    bool available = false;
    Power power = Power::Unknown;
    Label label;
    Label source;
    OptionList source_options;
    Label picture_mode;
    OptionList picture_mode_options;
    // Identyfikatory opcji w tej samej kolejności co listy wyżej: `hdmi1`,
    // `livetv`, `dolbyHdrCinema`. Nazwy zależą od TV i języka, polecenie idzie
    // po identyfikatorze (ADR 0008). Puste, gdy źródło nie rozróżnia jednych od drugich.
    OptionList source_ids;
    OptionList picture_mode_ids;
};

struct Z906State {
    bool available = false;
    Power power = Power::Unknown;
    bool muted = false;
    uint8_t volume_percent = 0;
    Label source;                 // jedna z `z906_sources()` (domain/z906_options.hpp)
    uint8_t center_percent = 0;
    uint8_t subwoofer_percent = 0;
    uint8_t rear_percent = 0;
    Label effect;                 // jedna z `z906_effects()`
    bool decode_5_1 = false;
};

// Bundle id aplikacji Apple TV. Bywa dłuższy niż etykieta, np. `com.google.ios.youtube`.
using AppId = core::Text<64>;

struct AppleTvApp {
    AppId id;
    Label label;   // nazwa z `source_list`, wyłącznie do wyświetlania
};

// Sekcja `appletv` kontraktu v1 (ADR 0007). Aplikację identyfikuje bundle id,
// bo nazwy zależą od wersji aplikacji i języka systemu.
struct AppleTvState {
    bool available = false;
    Power power = Power::Unknown;
    AppId app_id;   // pusty na ekranie głównym i w uśpieniu
    std::array<AppleTvApp, kMaxOptions> apps{};
    std::size_t app_count = 0;
    bool keyboard_active = false;

    bool has_app(const char* id) const {
        for (std::size_t i = 0; i < app_count; ++i) {
            if (apps[i].id == id) {
                return true;
            }
        }
        return false;
    }
};

// Klimatyzacja — encja `climate` w Home Assistancie. Tryby i opcje to
// identyfikatory HA (`cool`, `medlow`, `Full`), nazwy do pokazania dobiera ekran.
// Temperatury w dziesiątych częściach stopnia, żeby krok 0,5 nie wymagał liczb
// zmiennoprzecinkowych. Zakres i krok przychodzą z encji (`min_temp`, `max_temp`,
// `target_temp_step`), nie są zaszyte w firmware.
struct ClimateState {
    bool available = false;
    Label hvac_mode;              // `off`, gdy wyłączona
    OptionList hvac_modes;
    int16_t target_tenths = 0;
    int16_t current_tenths = 0;
    bool has_current = false;     // czujnik w pokoju bywa niedostępny osobno
    int16_t min_tenths = 160;
    int16_t max_tenths = 300;
    int16_t step_tenths = 5;
    Label fan_mode;
    OptionList fan_modes;
    Label swing_mode;             // żaluzje w pionie
    OptionList swing_modes;
    Label swing_horizontal_mode;  // żaluzje w poziomie
    OptionList swing_horizontal_modes;

    bool is_on() const { return !hvac_mode.empty() && hvac_mode != "off"; }
    // Temperatura docelowa nie działa po wyłączeniu ani w trybie samego nawiewu.
    bool uses_target() const { return is_on() && hvac_mode != "fan_only"; }
};

// ---------- Oświetlenie (ADR 0014) ----------
// Główne światło pomieszczenia. Encje zna wyłącznie bridge w Home Assistancie —
// pilot dostaje krótkie identyfikatory (`lampa`, `lampa_lewa`) i nimi steruje.
using LightId = core::Text<20>;
using EffectName = core::Text<16>;

inline constexpr std::size_t kMaxLightMembers = 4;   // żarówki w grupie
inline constexpr std::size_t kMaxRoomLights = 3;     // pomieszczenia ze światłem w snapshocie
inline constexpr std::size_t kMaxEffects = 16;       // effect_list WiZ ma dokładnie 16

struct LightState {
    LightId id;
    LightId label;
    bool available = false;
    bool on = false;
    uint8_t brightness = 0;   // 0..100 %; 0, gdy wyłączona
    uint16_t kelvin = 0;      // 0, gdy encja nie podaje (wyłączona albo w efekcie)
};

struct RoomLight {
    LightId room;             // identyfikator kafelka: `salon`, `kuchnia`…
    LightState main;          // encja główna (dla grupy: cała grupa)
    bool dimmable = false;    // `false` dla przełącznika: tylko wł./wył.
    uint16_t min_kelvin = 0;  // 0 = bez barwy
    uint16_t max_kelvin = 0;
    EffectName effect;        // pusty = bez efektu
    std::array<LightState, kMaxLightMembers> members{};
    std::size_t member_count = 0;

    bool has_kelvin() const { return min_kelvin > 0 && max_kelvin > min_kelvin; }
};

struct LightsState {
    bool available = false;   // bridge przysłał sekcję `lights`
    std::array<RoomLight, kMaxRoomLights> rooms{};
    std::size_t count = 0;
    // Lista efektów jest wspólna: bridge podaje ją z pierwszej grupy, która ją ma.
    std::array<EffectName, kMaxEffects> effects{};
    std::size_t effect_count = 0;

    const RoomLight* room(const char* id) const {
        for (std::size_t i = 0; i < count; ++i) {
            if (rooms[i].room == id) {
                return &rooms[i];
            }
        }
        return nullptr;
    }
};

// ---------- Grzejniki (ADR 0018) ----------
// Głowica termostatyczna w pomieszczeniu. Encję zna wyłącznie bridge — pilot steruje po `id`.
// Celowo bez temperatury głowicy: mierzy przy kaloryferze. Temperatura pokoju przychodzi
// z czujnika pokoju w wiadomości pogody (`indoor.rooms`, ADR 0017).
inline constexpr std::size_t kMaxRadiators = 4;
using RadiatorMode = core::Text<8>;   // `off`, `heat`, `auto`

struct RadiatorState {
    LightId room;                 // identyfikator pomieszczenia, jak w `lights`
    LightId id;
    bool available = false;
    RadiatorMode mode;
    bool has_heat = false;        // `modes` zawiera `heat`
    bool has_auto = false;        // `modes` zawiera `auto` (harmonogram w głowicy)
    int16_t target_tenths = 0;
    int16_t min_tenths = 40;
    int16_t max_tenths = 350;
    int16_t step_tenths = 5;
    bool heating = false;         // `hvac_action == heating`: zawór otwarty
    int8_t battery = -1;          // % baterii głowicy; -1 = nieznany
    int8_t window = -1;           // kontaktron okna pokoju: -1 brak czujnika, 0 zamknięte, 1 otwarte
    bool has_next = false;        // następny punkt harmonogramu, który zmienia temperaturę
    core::Text<6> next_at;        // `HH:MM`
    int16_t next_tenths = 0;

    bool is_on() const { return !mode.empty() && mode != "off"; }
    // Grzeje naprawdę: włączona, zawór otwarty i okno nie stoi otworem.
    bool heats() const { return available && is_on() && heating && window != 1; }
};

struct RadiatorsState {
    bool available = false;       // bridge przysłał sekcję `radiators`
    std::array<RadiatorState, kMaxRadiators> items{};
    std::size_t count = 0;

    const RadiatorState* room(const char* id) const {
        for (std::size_t i = 0; i < count; ++i) {
            if (items[i].room == id) {
                return &items[i];
            }
        }
        return nullptr;
    }
};

struct DeviceSnapshot {
    // Pilot ignoruje snapshot o rewizji nie większej niż ostatnio przetworzona.
    uint32_t revision = 0;
    TvState tv;
    Z906State z906;
    AppleTvState appletv;
    ClimateState climate;
    LightsState lights;
    RadiatorsState radiators;
};

}  // namespace domain
