#pragma once

// Powód ostatniego niepowodzenia połączenia — do pokazania na ekranie, nie do
// logu. Bez tego „brak połączenia" nie odróżnia złego adresu od złego hasła.

#include <cstdint>

namespace transport {

enum class MqttError : uint8_t {
    None,
    Unreachable,     // broker nie odpowiada
    NotAuthorized,   // broker odrzucił użytkownika lub hasło
    Rejected,        // odrzucony identyfikator albo wersja protokołu
    Other,
};

}  // namespace transport
