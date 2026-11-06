#include "board/board.hpp"

#include "bsp/esp-bsp.h"
#include "esp_log.h"

namespace board {
namespace {

constexpr char kTag[] = "board";

lv_display_t* g_display = nullptr;
Rotation g_rotation = Rotation::Portrait;
// Jasność wybrana w ustawieniach. Podświetlenie BSP po włączeniu zawsze daje
// 100 %, więc po wybudzeniu przywracamy tę wartość sami.
int g_brightness = 100;

lv_display_rotation_t to_lvgl(Rotation rotation) {
    switch (rotation) {
        case Rotation::Landscape90:
            return LV_DISPLAY_ROTATION_90;
        case Rotation::Landscape270:
            return LV_DISPLAY_ROTATION_270;
        case Rotation::Portrait:
        default:
            return LV_DISPLAY_ROTATION_0;
    }
}

}  // namespace

Rotation rotation_from(uint8_t stored) {
    switch (stored) {
        case static_cast<uint8_t>(Rotation::Landscape90):
            return Rotation::Landscape90;
        case static_cast<uint8_t>(Rotation::Landscape270):
            return Rotation::Landscape270;
        default:
            return Rotation::Portrait;
    }
}

Rotation rotation() {
    return g_rotation;
}

int32_t screen_width() {
    return g_rotation == Rotation::Portrait ? kScreenWidth : kScreenHeight;
}

int32_t screen_height() {
    return g_rotation == Rotation::Portrait ? kScreenHeight : kScreenWidth;
}

bool start(Rotation rotation) {
    if (g_display != nullptr) {
        return true;
    }

    // Bufor częściowy w PSRAM: panel ma 480 x 800, więc 80 linii to ok. 75 kB.
    // W poziomie ten sam bufor mieści 48 linii po 800 px.
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
    // Obrót tylko wtedy, gdy potrzebny: w pionie flush idzie prosto do panelu,
    // bez drugiego bufora i bez kosztu PPA.
    const bool rotated = rotation != Rotation::Portrait;
    cfg.flags.sw_rotate = rotated;

    g_display = bsp_display_start_with_config(&cfg);
    if (g_display == nullptr) {
        ESP_LOGE(kTag, "bsp_display_start_with_config zwrocilo NULL");
        return false;
    }

    if (rotated) {
        Lock lock(0);
        lv_display_set_rotation(g_display, to_lvgl(rotation));
    }
    g_rotation = rotation;

    bsp_display_backlight_on();
    ESP_LOGI(kTag, "ekran %dx%d gotowy, obrot %d", static_cast<int>(screen_width()),
             static_cast<int>(screen_height()), static_cast<int>(rotation));
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
