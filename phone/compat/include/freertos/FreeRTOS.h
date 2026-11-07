#pragma once

// FreeRTOS na wątkach systemu: zadania, kolejki i semafory w zakresie, którego używa
// firmware. Takt systemowy to 1 ms.

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t StackType_t;

#define pdFALSE 0
#define pdTRUE 1
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define configTICK_RATE_HZ 1000
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTICKS_TO_MS(ticks) ((uint32_t)(ticks))
#define tskNO_AFFINITY 0x7fffffff
#define tskIDLE_PRIORITY 0
#define configMAX_PRIORITIES 25
