#pragma once

// Bieżąca pogoda z Home Assistanta (ADR 0011). Treść, nie stan urządzenia: pilot trzyma
// ostatnią w pamięci i pokazuje ją od razu po wybudzeniu, bez synchronizacji.

#include <cstdint>

#include "domain/snapshot.hpp"

namespace domain {

struct Weather {
    bool known = false;
    Label condition;              // identyfikator HA: `sunny`, `partlycloudy`, `rainy`…
    int16_t temperature_tenths = 0;
    bool night = false;           // słońce pod horyzontem (`sun.sun` w HA)
    // W domu (ADR 0015): średnia z czujników pomieszczeń, liczona w bridge'u.
    bool indoor_known = false;
    int16_t indoor_tenths = 0;
    bool humidity_known = false;
    uint8_t humidity = 0;         // %
    int64_t received_ms = 0;      // czas pracy pilota przy odbiorze (core::now_ms)
    uint32_t revision = 0;        // rośnie z każdą nową wiadomością
};

// Po tylu milisekundach bez aktualizacji pogoda znika z ekranu zamiast udawać aktualną.
inline constexpr int64_t kWeatherMaxAgeMs = 6LL * 60 * 60 * 1000;

}  // namespace domain
