#pragma once

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct compat_queue* QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait);
BaseType_t xQueueSendToFront(QueueHandle_t queue, const void* item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t queue, void* out, TickType_t wait);
BaseType_t xQueuePeek(QueueHandle_t queue, void* out, TickType_t wait);
BaseType_t xQueueReset(QueueHandle_t queue);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);

#define xQueueSendToBack xQueueSend
#define xQueueOverwrite(queue, item) (xQueueReset(queue), xQueueSend(queue, item, 0))

#ifdef __cplusplus
}
#endif

#include "freertos/idf_additions.h"
