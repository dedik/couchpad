#pragma once

// Ustawienia urządzenia zapisane w NVS.
//
// Dane dostępowe do sieci są sekretem użytkownika: powstają na urządzeniu, leżą
// w jego pamięci i nie trafiają do repozytorium, logów ani przykładów.

#include <cstdint>

#include "core/text.hpp"

namespace storage {

using Ssid = core::Text<33>;
using Password = core::Text<65>;

struct WifiCredentials {
    Ssid ssid;
    Password password;
};

// Dostęp do brokera. Adres domowej infrastruktury nie należy do repozytorium —
// wpisuje się go na urządzeniu i leży wyłącznie w NVS.
struct MqttSettings {
    core::Text<64> host;
    uint16_t port = 1883;
    core::Text<33> username;
    core::Text<65> password;
    // Identyfikator kontrolera Z906 w temacie, postaci `z906_<mac>` (ADR 0006).
    // Odczytuje się go z tematów brokera, bo kontroler nadaje go sobie sam.
    core::Text<33> z906_id;
    // Identyfikator pilota w kontrakcie `multimedia_remote/v1/<remote_id>` (ADR 0007).
    // Pusty oznacza, że pilot nie rozmawia z Home Assistantem i Apple TV pozostaje
    // niedostępne — dźwięk działa bez niego.
    core::Text<33> remote_id;
    // Prefiks tematów Glasshouse na telewizorze (ADR 0008), domyślnie `lgtv`.
    // Pusty wyłącza telewizor — reszta pilota pracuje bez niego.
    core::Text<33> tv_prefix;

    bool complete() const { return !host.empty() && !z906_id.empty() && port != 0; }
    bool home_assistant() const { return !host.empty() && !remote_id.empty() && port != 0; }
    bool tv() const { return !host.empty() && !tv_prefix.empty() && port != 0; }
};

// Przygotowuje NVS. Wołać raz, przed pozostałymi funkcjami.
void init();

// `false`, gdy nic jeszcze nie zapisano.
bool load_wifi(WifiCredentials& out);
bool save_wifi(const WifiCredentials& credentials);
void clear_wifi();

bool load_mqtt(MqttSettings& out);
bool save_mqtt(const MqttSettings& settings);

// Preferencje jednobajtowe: klucz NVS i wartość domyślna żyją w settings.cpp.
// Odczyt idzie z pamięci podręcznej, więc wolno go wołać przy każdym rysowaniu.
enum class Pref : uint8_t {
    KeyboardQwerty,  // klawiatura QWERTY zamiast siatki jak na Apple TV
    KeyboardLive,    // tekst na Apple TV w trakcie pisania (domyślnie) albo po „Zapisz”
    Brightness,      // jasność ekranu w procentach, domyślnie 100
    ScreenTimeout,   // indeks czasu wygaszania (ui: 15 s, 30 s, 1 min, 2 min, nigdy), domyślnie 30 s
    DimBeforeSleep,  // przyciemnianie przed wygaszeniem, domyślnie wyłączone
    VolumeLimit,     // najwyższa głośność Z906 wysyłana z tarczy, domyślnie 100 (bez limitu)
    NightMode,       // indeks godzin trybu nocnego (ui), domyślnie 0 = wyłączony
    TimeZone,        // indeks strefy czasowej (ui), domyślnie 0 = Polska
    TrackpadStep,    // piksele ruchu na jeden krok gładzika, domyślnie 26
    KeyboardAutoOpen,// klawiatura wysuwa się sama, gdy Apple TV pokaże pole tekstowe; domyślnie tak
    ThemeMode,       // styl interfejsu (ui: ciemny, jasny, wg godzin), domyślnie 0 = ciemny
    ThemeHours,      // indeks godzin stylu ciemnego w trybie „wg godzin” (ui), domyślnie 18–8
    Orientation,     // obrót ekranu (board::Rotation): 0 pion, 1 poziom 90°, 2 poziom 270°; po zmianie restart
    Count,
};

uint8_t get(Pref pref);
void set(Pref pref, uint8_t value);

inline bool enabled(Pref pref) { return get(pref) != 0; }
inline void set_enabled(Pref pref, bool on) { set(pref, on ? 1 : 0); }

// Nazwa pokoju w nagłówku ekranów urządzeń. Pusta w NVS = „SALON”.
using RoomName = core::Text<25>;
void load_room(RoomName& out);
void save_room(const RoomName& room);

// Kasuje wszystkie ustawienia pilota, łącznie z Wi-Fi i brokerem.
bool factory_reset();

}  // namespace storage
