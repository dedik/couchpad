#pragma once

// Wejścia i efekty Logitech Z906. Są stałe dla tego modelu i kontroler ich nie
// publikuje, więc nie jadą w snapshocie: każda kopia stanu (kolejka klienta,
// LiveSource, AppState) niosła je po ok. 460 B na darmo. Napisy pokrywają się
// co do znaku z docs/integrations/z906.md i z wartościami tematów kontrolera.

#include "domain/snapshot.hpp"

namespace domain {

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
