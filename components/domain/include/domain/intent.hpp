#pragma once

// Zamiar użytkownika wyrażony dotykiem. Warstwa UI nie wie, czym jest MQTT —
// produkuje wyłącznie te struktury i oddaje je kontrolerowi aplikacji.

#include <cstdint>

#include "domain/snapshot.hpp"

namespace domain {

// Każdy identyfikator odpowiada jednemu sterowaniu na ekranie. Z niego bierze się
// stan oczekiwania i brak potwierdzenia, więc musi być stabilny i rozłączny.
enum class ControlId : uint8_t {
    None,
    TvPower,
    TvSource,
    TvPicture,
    TvChannel,
    TvChannelStep,  // program +/- (flag: w górę), chwilowe
    AtvPower,
    AtvApp,
    AtvKeyboard,
    ZPower,
    ZMute,
    ZVolume,
    ZCenter,
    ZSubwoofer,
    ZRear,
    ZSource,
    ZEffect,
    Scene,
    Shortcut,
    ClimatePower,
    ClimateMode,
    ClimateTarget,
    ClimateFan,
    ClimateSwing,
    ClimateSwingH,
    // Oświetlenie (ADR 0014). Identyfikator światła jedzie w `target`; oczekiwanie
    // zapamiętuje go w `text`, żeby pulsowało dokładnie to światło.
    LightPower,
    LightBrightness,  // value: 1..100 %
    LightKelvin,      // number: kelwiny
    LightEffect,      // payload: nazwa efektu z listy, pusty = bez efektu
};

enum class IntentKind : uint8_t {
    None,
    SetPower,       // control + flag
    SetMuted,       // flag
    SetValue,       // control + value (0..100)
    SetOption,      // control + text
    Navigate,       // key w text, bez stanu do potwierdzenia
    Momentary,      // skrót, scena, uruchomienie pozycji
    RequestSync,
};

enum class NavKey : uint8_t { None, Up, Down, Left, Right, Ok, Back, Home, Menu };

// Sposób naciśnięcia klawisza. Pilot Siri ma ukryte funkcje pod podwójnym
// stuknięciem i przytrzymaniem (Home ×2 — przełącznik aplikacji, przytrzymanie
// Home — Centrum sterowania, przytrzymanie OK — menu kontekstowe).
enum class Press : uint8_t { Tap, Double, Hold };

struct UiIntent {
    IntentKind kind = IntentKind::None;
    ControlId control = ControlId::None;
    bool flag = false;
    uint8_t value = 0;
    // Wartość spoza 0..100: temperatura klimatyzacji w dziesiątych stopnia.
    int16_t number = 0;
    NavKey key = NavKey::None;
    Press press = Press::Tap;
    Label text;      // nazwa opcji, identyfikator sceny lub pozycji
    Label target;    // urządzenie dla nawigacji: "tv" albo "appletv"
    // Wartość dłuższa niż etykieta: bundle id aplikacji Apple TV albo tekst
    // klawiatury. Pusty tekst klawiatury oznacza wyczyszczenie pola.
    core::Text<64> payload;
};

// `true` dla zamiarów telewizora, które idą wprost do Glasshouse (ADR 0008).
// Skrót kanału do nich nie należy: Glasshouse nie zna go po MQTT, więc idzie
// kontraktem v1 do Home Assistanta. Program +/- to zwykły klawisz pilota.
inline bool targets_tv(const UiIntent& intent) {
    switch (intent.control) {
        case ControlId::TvPower:
        case ControlId::TvSource:
        case ControlId::TvPicture:
        case ControlId::TvChannelStep:
            return true;
        default:
            return intent.kind == IntentKind::Navigate && intent.target == "tv";
    }
}

// `true` dla zamiarów, które obsługuje Apple TV przez Home Assistanta (ADR 0007).
inline bool targets_appletv(const UiIntent& intent) {
    switch (intent.control) {
        case ControlId::AtvPower:
        case ControlId::AtvApp:
        case ControlId::AtvKeyboard:
            return true;
        default:
            return intent.kind == IntentKind::Navigate && intent.target == "appletv";
    }
}

// `true` dla zamiarów klimatyzacji. Idą kontraktem v1 do Home Assistanta (ADR 0009).
inline bool targets_climate(const UiIntent& intent) {
    switch (intent.control) {
        case ControlId::ClimatePower:
        case ControlId::ClimateMode:
        case ControlId::ClimateTarget:
        case ControlId::ClimateFan:
        case ControlId::ClimateSwing:
        case ControlId::ClimateSwingH:
            return true;
        default:
            return false;
    }
}

// `true` dla zamiarów oświetlenia. Idą kontraktem v1 do Home Assistanta (ADR 0014).
inline bool targets_lights(const UiIntent& intent) {
    switch (intent.control) {
        case ControlId::LightPower:
        case ControlId::LightBrightness:
        case ControlId::LightKelvin:
        case ControlId::LightEffect:
            return true;
        default:
            return false;
    }
}

// `true` dla zamiarów, które obsługuje Home Assistant kontraktem v1: Apple TV
// (ADR 0007) i skróty kanałów telewizora (ADR 0008).
inline bool targets_home_assistant(const UiIntent& intent) {
    return intent.control == ControlId::TvChannel || targets_appletv(intent);
}

}  // namespace domain
