#pragma once

#include <cstdint>

namespace core {

// Milisekundy od startu. Jedno źródło czasu dla limitów oczekiwania na snapshot.
int64_t now_ms();

}  // namespace core
