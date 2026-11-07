#pragma once

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct compat_task* TaskHandle_t;
typedef void (*TaskFunction_t)(void* arg);

BaseType_t xTaskCreate(TaskFunction_t entry, const char* name, uint32_t stack, void* arg,
                       UBaseType_t priority, TaskHandle_t* out);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char* name, uint32_t stack,
                                   void* arg, UBaseType_t priority, TaskHandle_t* out,
                                   BaseType_t core);
// Kończy bieżące zadanie (tylko `NULL`, jak w firmware).
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);

#ifdef __cplusplus
}
#endif

#include "freertos/idf_additions.h"
