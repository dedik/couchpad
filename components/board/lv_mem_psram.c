/* Alokator LVGL w PSRAM (CONFIG_LV_USE_CUSTOM_MALLOC).
 *
 * Domyślny `malloc` kieruje każdą alokację do 4 kB (SPIRAM_MALLOC_ALWAYSINTERNAL) do
 * pamięci wewnętrznej — a tyle waży niemal każdy obiekt LVGL. Pięć ekranów zjadało przez
 * to ok. 173 kB z ~450 kB wewnętrznych (pomiar 2026-11-03). Obiekty, style i teksty LVGL
 * czyta wyłącznie procesor przez pamięć podręczną, więc PSRAM im wystarcza. Bufor rysowania
 * przydziela osobno esp_lvgl_port (też w PSRAM, board.cpp).
 *
 * Gdy PSRAM się skończy, alokacja wraca do zwykłej sterty, zamiast zawieść.
 */
#include "esp_heap_caps.h"
#include "lvgl.h"

#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void lv_mem_init(void) {}

void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void * mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void * lv_malloc_core(size_t size)
{
    void * p = heap_caps_malloc(size, PSRAM_CAPS);
    return p != NULL ? p : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void * lv_realloc_core(void * p, size_t new_size)
{
    void * moved = heap_caps_realloc(p, new_size, PSRAM_CAPS);
    return moved != NULL ? moved : heap_caps_realloc(p, new_size, MALLOC_CAP_8BIT);
}

void lv_free_core(void * p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t * mon_p)
{
    LV_UNUSED(mon_p);
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}
