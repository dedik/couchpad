#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Firmware restartuje się np. po zmianie orientacji. Na telefonie platforma decyduje,
// co to znaczy (odbudowa interfejsu albo zamknięcie aplikacji).
void esp_restart(void) __attribute__((noreturn));

typedef void (*compat_restart_handler_t)(void);
void compat_set_restart_handler(compat_restart_handler_t handler);

#ifdef __cplusplus
}
#endif
