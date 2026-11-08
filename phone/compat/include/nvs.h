#pragma once

// NVS w pliku w prywatnym katalogu aplikacji. Ustawienia (Wi-Fi nie dotyczy telefonu,
// ale broker MQTT tak) zostają na urządzeniu, jak na płytce — nigdy w repozytorium.

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

esp_err_t nvs_open(const char* name_space, nvs_open_mode_t mode, nvs_handle_t* out);
void nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);
esp_err_t nvs_erase_all(nvs_handle_t handle);

esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out, size_t* length);
esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_get_u16(nvs_handle_t handle, const char* key, uint16_t* out);
esp_err_t nvs_set_u16(nvs_handle_t handle, const char* key, uint16_t value);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char* key, uint32_t* out);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char* key, uint32_t value);

// Katalog na plik ustawień; wołać przed nvs_flash_init.
void compat_nvs_set_dir(const char* directory);

#ifdef __cplusplus
}
#endif
