#pragma once

// Semafory to kolejki z elementem zerowej długości, jak w samym FreeRTOS.
// Mutex nie śledzi właściciela — firmware nie bierze go rekurencyjnie.

#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef QueueHandle_t SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore);
#define vSemaphoreDelete(semaphore) vQueueDelete(semaphore)

#ifdef __cplusplus
}
#endif
