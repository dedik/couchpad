#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef const char* esp_event_base_t;
typedef void* esp_event_handler_instance_t;
typedef void (*esp_event_handler_t)(void* arg, esp_event_base_t base, int32_t id, void* data);

extern const esp_event_base_t IP_EVENT;
enum { IP_EVENT_STA_GOT_IP = 0, IP_EVENT_STA_LOST_IP = 1 };

#define ESP_EVENT_ANY_ID -1

esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
                                              esp_event_handler_t handler, void* arg,
                                              esp_event_handler_instance_t* instance);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
                                                esp_event_handler_instance_t instance);
esp_err_t esp_event_post(esp_event_base_t base, int32_t id, const void* data, size_t size,
                         uint32_t ticks);

#ifdef __cplusplus
}
#endif
