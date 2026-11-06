#pragma once

// Wejścia i efekty Logitech Z906. Są stałe dla tego modelu i kontroler ich nie
// publikuje, więc nie jadą w snapshocie: każda kopia stanu (kolejka klienta,
// LiveSource, AppState) niosła je po ok. 460 B na darmo. Napisy pokrywają się
// co do znaku z docs/integrations/z906.md i z wartościami tematów kontrolera.

#include <cstdint>

#include "domain/snapshot.hpp"

namespace domain {

// Wzmacniacz ma 44 poziomy głośności (0–43). Kontroler przyjmuje procent,
// zaokrągla go do najbliższego poziomu i odsyła poziom z powrotem jako procent,
// więc osiągalne są tylko wartości 0, 2, 5, 7, 9, 12, 14, 16, 19, 21, 23, 26…
// Potwierdzone historią HA 2026-11-05. Krok o 1 % zwykle nie zmienia poziomu:
// kontroler zostaje na starym, nie przychodzi nowy snapshot i pilot pokazuje
// brak potwierdzenia. Dlatego −/+ liczą w poziomach, nie w procentach.
constexpr int32_t kZ906MaxLevel = 43;

constexpr int32_t z906_level_of(int32_t percent) {
    return (percent * kZ906MaxLevel + 50) / 100;
}

constexpr int32_t z906_percent_of(int32_t level) {
    return (level * 100 + kZ906MaxLevel / 2) / kZ906MaxLevel;
}

// Następna osiągalna głośność o `levels` poziomów od `percent`, nie wyżej niż
// `limit`. Zwraca `percent`, gdy w danym kierunku nie ma już dokąd iść.
constexpr int32_t z906_step_volume(int32_t percent, int32_t levels, int32_t limit) {
    int32_t level = z906_level_of(percent) + levels;
    level = level < 0 ? 0 : (level > kZ906MaxLevel ? kZ906MaxLevel : level);
    int32_t next = z906_percent_of(level);
    if (next > limit) {
        // Najwyższy poziom, którego procent mieści się w limicie.
        next = z906_percent_of(limit * kZ906MaxLevel / 100);
    }
    // Limit poza siatką poziomów nie może sprawić, że „+” ścisza albo „−” podgłaśnia.
    if ((levels > 0 && next <= percent) || (levels < 0 && next >= percent)) {
        return percent;
    }
    return next;
}

static_assert(z906_percent_of(13) == 30 && z906_percent_of(14) == 33, "siatka poziomów Z906");
static_assert(z906_level_of(z906_percent_of(21)) == 21, "poziom wraca z procentu");
static_assert(z906_step_volume(28, 1, 100) == 30 && z906_step_volume(28, -1, 100) == 26,
              "krok o jeden poziom");
static_assert(z906_step_volume(50, 1, 50) == 50, "limit poza siatką nie ścisza");

inline const OptionList& z906_sources() {
    static const OptionList list = [] {
        OptionList out;
        for (const char* source :
             {"PC - analog 5.1", "RCA - stereo", "Optical 1", "Optical 2", "Coaxial", "AUX"}) {
            out.add(source);
        }
        return out;
    }();
    return list;
}

inline const OptionList& z906_effects() {
    static const OptionList list = [] {
        OptionList out;
        for (const char* effect : {"Stereo 3D", "Stereo 2.1", "Stereo 4.1", "No effect"}) {
            out.add(effect);
        }
        return out;
    }();
    return list;
}

}  // namespace domain
