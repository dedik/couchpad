#pragma once

// Warianty ESP-IDF z wyborem pamięci. Na telefonie pamięć jest jedna.

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

struct compat_task;
struct compat_queue;

BaseType_t xTaskCreatePinnedToCoreWithCaps(void (*entry)(void*), const char* name,
                                           uint32_t stack, void* arg, UBaseType_t priority,
                                           struct compat_task** out, BaseType_t core,
                                           uint32_t caps);
struct compat_queue* xQueueCreateWithCaps(UBaseType_t length, UBaseType_t item_size,
                                          uint32_t caps);

#ifdef __cplusplus
}
#endif
