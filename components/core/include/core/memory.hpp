#pragma once

// Pomiar pamięci na czas strojenia: ile zostało RAM-u wewnętrznego i PSRAM,
// jak duży jest największy wolny blok i ile zapasu ma stos każdego zadania.
// Pamięć wewnętrzna (~768 kB) jest tu zasobem krytycznym, PSRAM (32 MB) — nie.

#include <new>
#include <utility>

#include "esp_heap_caps.h"

namespace core::memory {

// Obiekt na cały czas pracy, w PSRAM zamiast w .bss pamięci wewnętrznej. Dla dużych
// struktur stanu (snapshoty, lista kanałów, kontekst interfejsu), które czyta tylko
// procesor. Nigdy nie jest zwalniany. Bez PSRAM ląduje na zwykłej stercie.
template <typename T, typename... Args>
T& make_in_psram(Args&&... args) {
    void* raw = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (raw == nullptr) {
        raw = heap_caps_malloc(sizeof(T), MALLOC_CAP_8BIT);
    }
    return *new (raw) T(std::forward<Args>(args)...);
}

// Jedna linia: etap, wolna pamięć wewnętrzna, największy blok, minimum od startu, PSRAM.
void log_stage(const char* stage);

// Zapas stosu (najmniej wolnego od startu) każdego zadania FreeRTOS, od najciaśniejszego.
void log_tasks();

}  // namespace core::memory
