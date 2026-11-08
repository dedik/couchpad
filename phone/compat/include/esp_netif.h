#pragma once

// Sieć na telefonie należy do systemu. Interfejs „stacji” istnieje zawsze, a jego adres
// to lokalny adres, z którego telefon wychodzi do sieci.

#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct compat_netif esp_netif_t;

typedef struct {
    uint32_t addr;  // kolejność bajtów sieciowa, jak w lwIP
} esp_ip4_addr_t;

typedef struct {
    esp_ip4_addr_t ip;
    esp_ip4_addr_t netmask;
    esp_ip4_addr_t gw;
} esp_netif_ip_info_t;

esp_netif_t* esp_netif_get_handle_from_ifkey(const char* key);
esp_err_t esp_netif_get_ip_info(esp_netif_t* netif, esp_netif_ip_info_t* info);

#define IP2STR(ipaddr)                                                               \
    ((uint8_t*)(&(ipaddr)->addr))[0], ((uint8_t*)(&(ipaddr)->addr))[1],             \
        ((uint8_t*)(&(ipaddr)->addr))[2], ((uint8_t*)(&(ipaddr)->addr))[3]
#define IPSTR "%d.%d.%d.%d"

#ifdef __cplusplus
}
#endif
