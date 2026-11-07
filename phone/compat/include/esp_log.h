#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE,
} esp_log_level_t;

void compat_log(esp_log_level_t level, const char* tag, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

static inline void esp_log_level_set(const char* tag, esp_log_level_t level) {
    (void)tag;
    (void)level;
}

#ifdef __cplusplus
}
#endif

#define ESP_LOGE(tag, format, ...) compat_log(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) compat_log(ESP_LOG_WARN, tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) compat_log(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) compat_log(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) compat_log(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__)
