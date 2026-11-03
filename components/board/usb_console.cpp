// Konsola USB na czas rozwoju (CONFIG_PILOT_USB_CONSOLE): zrzut ekranu i zdalny dotyk.
//
// Komputer wysyła po USB-Serial-JTAG linie tekstu:
//
//   shot                         bieżąca ramka panelu w base64 między `@@SHOT` i `@@END`
//   tap <x> <y>                  krótkie dotknięcie
//   drag <x1> <y1> <x2> <y2> <ms>  przeciągnięcie palcem
//
// Zrzut to kopia bufora ramki panelu DPI — dokładnie to, co widać, razem z nakładkami
// z lv_layer_top i lv_layer_sys. Dotyk idzie przez osobne wejście LVGL obok GT911,
// więc ekran reaguje na niego tak jak na palec. Obsługuje to tools/remote.py.
//
// Na czas wysyłki ramki log milknie: linie ESP_LOG idą tym samym portem i rozbiłyby ją.

#include "board/board.hpp"

#include <cstdio>
#include <cstring>

#include "bsp/esp-bsp.h"
#include "driver/usb_serial_jtag.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace board {
namespace {

constexpr char kTag[] = "console";
constexpr std::size_t kFrameBytes =
    static_cast<std::size_t>(kScreenWidth) * static_cast<std::size_t>(kScreenHeight) * 2;
// 3 kB surowych danych na linię: 4 kB tekstu, tyle ile bufor nadawczy sterownika.
constexpr std::size_t kChunk = 3 * 1024;
constexpr std::size_t kTxBuffer = 4 * 1024 + 64;
constexpr TickType_t kWriteTimeout = pdMS_TO_TICKS(2000);
// Dotknięcie musi przeżyć co najmniej dwa odczyty wejścia LVGL (co 30 ms).
constexpr uint32_t kTapMs = 90;
constexpr uint32_t kDragStepMs = 15;

// ---------- zdalny dotyk ----------

// Stan palca czytany przez zadanie LVGL; pisze go tylko zadanie konsoli.
struct Finger {
    volatile int32_t x = 0;
    volatile int32_t y = 0;
    volatile bool pressed = false;
};
Finger g_finger;

void finger_read(lv_indev_t*, lv_indev_data_t* data) {
    data->point.x = g_finger.x;
    data->point.y = g_finger.y;
    data->state = g_finger.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void move_finger(int32_t x, int32_t y, bool pressed) {
    g_finger.x = x;
    g_finger.y = y;
    g_finger.pressed = pressed;
}

void tap(int32_t x, int32_t y) {
    move_finger(x, y, true);
    vTaskDelay(pdMS_TO_TICKS(kTapMs));
    move_finger(x, y, false);
    vTaskDelay(pdMS_TO_TICKS(kTapMs));
}

void drag(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t ms) {
    const int32_t steps = static_cast<int32_t>(ms / kDragStepMs) + 1;
    move_finger(x1, y1, true);
    vTaskDelay(pdMS_TO_TICKS(kTapMs));
    for (int32_t i = 1; i <= steps; ++i) {
        move_finger(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps, true);
        vTaskDelay(pdMS_TO_TICKS(kDragStepMs));
    }
    vTaskDelay(pdMS_TO_TICKS(kTapMs));
    move_finger(x2, y2, false);
    vTaskDelay(pdMS_TO_TICKS(kTapMs));
}

// ---------- zrzut ekranu ----------

int mute(const char*, va_list) { return 0; }

bool write_all(const char* data, std::size_t size) {
    while (size > 0) {
        const int written = usb_serial_jtag_write_bytes(data, size, kWriteTimeout);
        if (written <= 0) {
            return false;  // nikt nie czyta portu
        }
        data += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

std::size_t base64(const uint8_t* in, std::size_t size, char* out) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char* start = out;
    for (std::size_t i = 0; i < size; i += 3) {
        const uint32_t a = in[i];
        const uint32_t b = i + 1 < size ? in[i + 1] : 0;
        const uint32_t c = i + 2 < size ? in[i + 2] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;
        *out++ = kAlphabet[(triple >> 18) & 0x3F];
        *out++ = kAlphabet[(triple >> 12) & 0x3F];
        *out++ = i + 1 < size ? kAlphabet[(triple >> 6) & 0x3F] : '=';
        *out++ = i + 2 < size ? kAlphabet[triple & 0x3F] : '=';
    }
    return static_cast<std::size_t>(out - start);
}

// Kopia ramki pod blokadą LVGL, żeby nie złapać jej w połowie odświeżania.
// Wysyłka trwa około sekundy, więc idzie już bez blokady.
uint8_t* copy_frame() {
    auto* copy = static_cast<uint8_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM));
    if (copy == nullptr) {
        return nullptr;
    }
    Lock lock(1000);
    void* frame = lock.held() ? bsp_display_get_frame_buffer() : nullptr;
    if (frame == nullptr) {
        heap_caps_free(copy);
        return nullptr;
    }
    // Ramkę może zapisać DMA2D, z pominięciem pamięci podręcznej procesora.
    esp_cache_msync(frame, kFrameBytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    std::memcpy(copy, frame, kFrameBytes);
    return copy;
}

void send_frame() {
    uint8_t* frame = copy_frame();
    if (frame == nullptr) {
        ESP_LOGW(kTag, "nie ma ramki do wyslania");
        return;
    }
    static char line[kChunk / 3 * 4 + 2];
    const vprintf_like_t previous = esp_log_set_vprintf(&mute);

    int length = snprintf(line, sizeof(line), "\n@@SHOT %d %d RGB565 %u\n",
                          static_cast<int>(kScreenWidth), static_cast<int>(kScreenHeight),
                          static_cast<unsigned>(kFrameBytes));
    bool ok = write_all(line, static_cast<std::size_t>(length));
    for (std::size_t offset = 0; ok && offset < kFrameBytes; offset += kChunk) {
        const std::size_t size = kFrameBytes - offset < kChunk ? kFrameBytes - offset : kChunk;
        std::size_t used = base64(frame + offset, size, line);
        line[used++] = '\n';
        ok = write_all(line, used);
    }
    if (ok) {
        length = snprintf(line, sizeof(line), "@@END %08lx\n",
                          static_cast<unsigned long>(esp_rom_crc32_le(0, frame, kFrameBytes)));
        ok = write_all(line, static_cast<std::size_t>(length));
    }

    esp_log_set_vprintf(previous);
    heap_caps_free(frame);
    if (!ok) {
        ESP_LOGW(kTag, "zrzut przerwany: port przestal odbierac");
    }
}

// ---------- komendy ----------

void run(const char* command) {
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    int ms = 0;
    if (std::strcmp(command, "shot") == 0) {
        send_frame();
    } else if (std::sscanf(command, "tap %d %d", &x1, &y1) == 2) {
        tap(x1, y1);
    } else if (std::sscanf(command, "drag %d %d %d %d %d", &x1, &y1, &x2, &y2, &ms) == 5) {
        drag(x1, y1, x2, y2, static_cast<uint32_t>(ms));
    } else {
        return;
    }
    // Komputer czeka na to potwierdzenie, zanim wyśle kolejną komendę.
    static const char kDone[] = "@@OK\n";
    write_all(kDone, sizeof(kDone) - 1);
}

void console_task(void*) {
    char command[48];
    std::size_t used = 0;
    for (;;) {
        char byte = 0;
        if (usb_serial_jtag_read_bytes(&byte, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (byte != '\n' && byte != '\r') {
            if (used < sizeof(command) - 1) {
                command[used++] = byte;
            }
            continue;
        }
        command[used] = '\0';
        used = 0;
        run(command);
    }
}

}  // namespace

void start_usb_console() {
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    config.tx_buffer_size = kTxBuffer;
    if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
        ESP_LOGW(kTag, "sterownik USB-Serial-JTAG nie ruszyl — konsoli nie bedzie");
        return;
    }
    {
        Lock lock(0);
        lv_indev_t* finger = lv_indev_create();
        lv_indev_set_type(finger, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(finger, &finger_read);
    }
    xTaskCreate(&console_task, "console", 4096, nullptr, 2, nullptr);
    ESP_LOGI(kTag, "konsola USB gotowa: zrzuty i dotyk (tools/remote.py)");
}

}  // namespace board
