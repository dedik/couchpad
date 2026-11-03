#include "board/board.hpp"

#include "bsp/esp-bsp.h"
#include "esp_log.h"

namespace board {
namespace {

constexpr char kTag[] = "board";

lv_display_t* g_display = nullptr;
// Jasność wybrana w ustawieniach. Podświetlenie BSP po włączeniu zawsze daje
// 100 %, więc po wybudzeniu przywracamy tę wartość sami.
int g_brightness = 100;

}  // namespace

bool start() {
    if (g_display != nullptr) {
        return true;
    }

    // Bufor częściowy w PSRAM: panel ma 480 x 800, więc 80 linii to ok. 75 kB.
    // Jeden bufor ramki DPI po stronie sterownika — tak potwierdzono przy bring-upie.
    bsp_display_cfg_t cfg = {};
    cfg.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    cfg.lvgl_port_cfg.task_priority = 4;
    // Zadanie LVGL opróżnia też kolejki źródeł, a snapshot waży ok. 2,5 kB.
    // 8 kB nie starczyło po dołożeniu Apple TV (przepełnienie stosu przy starcie).
    cfg.lvgl_port_cfg.task_stack = 12288;
    cfg.lvgl_port_cfg.timer_period_ms = 5;
    cfg.buffer_size = BSP_LCD_H_RES * 80;
    cfg.double_buffer = false;
    cfg.flags.buff_dma = false;
    cfg.flags.buff_spiram = true;
    cfg.flags.sw_rotate = false;

    g_display = bsp_display_start_with_config(&cfg);
    if (g_display == nullptr) {
        ESP_LOGE(kTag, "bsp_display_start_with_config zwrocilo NULL");
        return false;
    }

    bsp_display_backlight_on();
    ESP_LOGI(kTag, "ekran %dx%d gotowy", static_cast<int>(kScreenWidth), static_cast<int>(kScreenHeight));
    return true;
}

void set_brightness(int percent) {
    g_brightness = percent;
    bsp_display_brightness_set(percent);
}

void dim(int percent) {
    bsp_display_brightness_set(percent < g_brightness ? percent : g_brightness);
}

void set_display_on(bool on) {
    if (on) {
        bsp_display_brightness_set(g_brightness);
    } else {
        bsp_display_backlight_off();
    }
}

bool lock(uint32_t timeout_ms) {
    return bsp_display_lock(timeout_ms);
}

void unlock() {
    bsp_display_unlock();
}

}  // namespace board
