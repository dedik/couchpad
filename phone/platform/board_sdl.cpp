// board.hpp na SDL2: okno (Mac) albo pełny ekran (Android), dotyk i podświetlenie.
//
// Interfejs renderuje się w natywnych pikselach płytki (480 × 800 albo 800 × 480),
// a SDL skaluje go do ekranu telefonu z zachowaniem proporcji. Na Galaxy S5 (1080 × 1920)
// to prawie cały ekran; na wyższych telefonach zostają pasy u góry i na dole.

#include <SDL.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "board/board.hpp"
#include "board_phone.hpp"
#include "esp_log.h"
#include "lvgl.h"

namespace board {
namespace {

constexpr char kTag[] = "board";

std::recursive_mutex g_lvgl_mutex;
Rotation g_rotation = Rotation::Portrait;
int32_t g_width = kScreenWidth;
int32_t g_height = kScreenHeight;

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
lv_display_t* g_display = nullptr;
lv_indev_t* g_pointer = nullptr;
std::vector<uint16_t> g_framebuffer;

int g_brightness = 100;
int g_level = 100;  // bieżąca jasność (po przyciemnieniu)
bool g_display_on = true;
bool g_quit = false;
bool g_foreground = true;

int32_t g_touch_x = 0;
int32_t g_touch_y = 0;
bool g_touch_down = false;

uint32_t tick_cb() { return SDL_GetTicks(); }

void log_cb(lv_log_level_t level, const char* text) {
    compat_log(level >= LV_LOG_LEVEL_ERROR  ? ESP_LOG_ERROR
               : level >= LV_LOG_LEVEL_WARN ? ESP_LOG_WARN
                                            : ESP_LOG_INFO,
               "lvgl", "%s", text);
}

void present() {
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    if (g_display_on) {
        // Jasność jako przyciemnienie obrazu; podświetlenie telefonu zostaje systemowi.
        const uint8_t level = static_cast<uint8_t>(255 * (g_level < 5 ? 5 : g_level) / 100);
        SDL_SetTextureColorMod(g_texture, level, level, level);
        SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
    }
    SDL_RenderPresent(g_renderer);
}

void flush_cb(lv_display_t* display, const lv_area_t*, uint8_t*) {
    // Tryb DIRECT: LVGL rysuje wprost w g_framebuffer. Po ostatnim obszarze ramki
    // wysyłamy całą klatkę do tekstury.
    if (lv_display_flush_is_last(display)) {
        SDL_UpdateTexture(g_texture, nullptr, g_framebuffer.data(),
                          static_cast<int>(g_width * sizeof(uint16_t)));
        present();
    }
    lv_display_flush_ready(display);
}

// Zmiany stanu dotyku czekają w kolejce na odczyt LVGL (co ~30 ms). Bez niej krótkie
// stuknięcie — wciśnięcie i puszczenie między dwoma odczytami — przepadało.
struct TouchSample {
    int32_t x;
    int32_t y;
    bool down;
};
std::vector<TouchSample> g_touch_queue;

void read_cb(lv_indev_t*, lv_indev_data_t* data) {
    if (!g_touch_queue.empty()) {
        const TouchSample sample = g_touch_queue.front();
        g_touch_queue.erase(g_touch_queue.begin());
        data->point.x = sample.x;
        data->point.y = sample.y;
        data->state = sample.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = !g_touch_queue.empty();
        return;
    }
    data->point.x = g_touch_x;
    data->point.y = g_touch_y;
    data->state = g_touch_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void set_touch(float x, float y, bool down) {
    // Współrzędne logiczne: SDL_RenderSetLogicalSize przelicza je za nas.
    int32_t px = static_cast<int32_t>(x);
    int32_t py = static_cast<int32_t>(y);
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= g_width) px = g_width - 1;
    if (py >= g_height) py = g_height - 1;
    // Ruch przy wciśniętym palcu nie musi czekać w kolejce — liczy się ostatnia pozycja.
    // Zmiana wciśnięty/puszczony zawsze trafia do kolejki.
    if (down != g_touch_down || !g_touch_queue.empty()) {
        if (g_touch_queue.size() < 64) {
            g_touch_queue.push_back(TouchSample{px, py, down});
        }
    }
    g_touch_x = px;
    g_touch_y = py;
    g_touch_down = down;
}

void handle_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
        case SDL_APP_TERMINATING:
            g_quit = true;
            break;
        case SDL_APP_WILLENTERBACKGROUND:
            g_foreground = false;
            break;
        case SDL_APP_DIDENTERFOREGROUND:
            g_foreground = true;
            if (g_display != nullptr) {
                lv_obj_invalidate(lv_screen_active());
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                set_touch(event.button.x, event.button.y, true);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                set_touch(event.button.x, event.button.y, false);
            }
            break;
        case SDL_MOUSEMOTION:
            if (g_touch_down) {
                set_touch(event.motion.x, event.motion.y, true);
            }
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                present();
            }
            break;
        case SDL_KEYDOWN:
            // Wstecz na Androidzie nie zamyka pilota.
            if (event.key.keysym.sym == SDLK_AC_BACK) {
                break;
            }
            break;
        default:
            break;
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

Rotation rotation() { return g_rotation; }
int32_t screen_width() { return g_width; }
int32_t screen_height() { return g_height; }

bool start(Rotation rotation) {
    g_rotation = rotation;
    const bool landscape = rotation != Rotation::Portrait;
    g_width = landscape ? kScreenHeight : kScreenWidth;
    g_height = landscape ? kScreenWidth : kScreenHeight;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, landscape ? "LandscapeLeft LandscapeRight" : "Portrait");
    // Dotyk przychodzi jako zdarzenia myszy — jedna ścieżka dla Maca i Androida.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        ESP_LOGE(kTag, "SDL_Init: %s", SDL_GetError());
        return false;
    }

#ifdef __ANDROID__
    const Uint32 flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
    const int window_w = 0;
    const int window_h = 0;
#else
    const Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    const int window_w = g_width;
    const int window_h = g_height;
#endif
    g_window = SDL_CreateWindow("Pilot", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_w,
                                window_h, flags);
    if (g_window == nullptr) {
        ESP_LOGE(kTag, "SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == nullptr) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (g_renderer == nullptr) {
        ESP_LOGE(kTag, "SDL_CreateRenderer: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(g_renderer, g_width, g_height);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                  g_width, g_height);
    if (g_texture == nullptr) {
        ESP_LOGE(kTag, "SDL_CreateTexture: %s", SDL_GetError());
        return false;
    }
    SDL_DisableScreenSaver();

    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_log_register_print_cb(log_cb);
    g_display = lv_display_create(g_width, g_height);
    lv_display_set_color_format(g_display, LV_COLOR_FORMAT_RGB565);
    g_framebuffer.assign(static_cast<size_t>(g_width) * g_height, 0);
    lv_display_set_buffers(g_display, g_framebuffer.data(), nullptr,
                           static_cast<uint32_t>(g_framebuffer.size() * sizeof(uint16_t)),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(g_display, flush_cb);

    g_pointer = lv_indev_create();
    lv_indev_set_type(g_pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_pointer, read_cb);
    lv_indev_set_display(g_pointer, g_display);

    int out_w = 0;
    int out_h = 0;
    SDL_GetRendererOutputSize(g_renderer, &out_w, &out_h);
    ESP_LOGI(kTag, "ekran %dx%d na wyjsciu %dx%d", static_cast<int>(g_width),
             static_cast<int>(g_height), out_w, out_h);
    return true;
}

void set_brightness(int percent) {
    g_brightness = percent < 1 ? 1 : percent > 100 ? 100 : percent;
    g_level = g_brightness;
}

void dim(int percent) { g_level = percent; }

void set_display_on(bool on) {
    g_display_on = on;
    if (on) {
        g_level = g_brightness;
        // Ekran telefonu ma nie gasnąć, dopóki pilot jest aktywny.
        SDL_DisableScreenSaver();
    } else {
        // Pilot „zasnął”: oddajemy ekran systemowi, który wygasi go po swoim czasie.
        SDL_EnableScreenSaver();
    }
    present();
}

void start_usb_console() {}

bool lock(uint32_t) {
    g_lvgl_mutex.lock();
    return true;
}

void unlock() { g_lvgl_mutex.unlock(); }

// Zrzut bieżącej klatki do BMP (Mac, weryfikacja bez patrzenia na okno):
// PILOT_SHOT=plik.bmp, PILOT_SHOT_AFTER_MS=opóźnienie, PILOT_SHOT_QUIT=1 kończy program.
// PILOT_TAP="x,y;x,y" klika kolejno w podane punkty co sekundę przed zrzutem.
void save_shot(const char* path) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        g_framebuffer.data(), g_width, g_height, 16, static_cast<int>(g_width * sizeof(uint16_t)),
        SDL_PIXELFORMAT_RGB565);
    if (surface != nullptr) {
        SDL_SaveBMP(surface, path);
        SDL_FreeSurface(surface);
        ESP_LOGI(kTag, "zrzut zapisany: %s", path);
    }
}

void run_loop() {
    const char* shot = SDL_getenv("PILOT_SHOT");
    const char* after = SDL_getenv("PILOT_SHOT_AFTER_MS");
    const char* quit_after_shot = SDL_getenv("PILOT_SHOT_QUIT");
    const char* taps = SDL_getenv("PILOT_TAP");
    std::vector<std::pair<int, int>> tap_points;
    if (taps != nullptr) {
        const char* p = taps;
        while (*p != '\0') {
            int x = 0;
            int y = 0;
            if (SDL_sscanf(p, "%d,%d", &x, &y) == 2) tap_points.emplace_back(x, y);
            const char* next = SDL_strchr(p, ';');
            if (next == nullptr) break;
            p = next + 1;
        }
    }
    size_t next_tap = 0;
    const Uint32 started = SDL_GetTicks();
    const Uint32 shot_at = started + (after != nullptr ? static_cast<Uint32>(SDL_atoi(after)) : 3000) +
                           static_cast<Uint32>(tap_points.size()) * 1000;
    bool shot_done = shot == nullptr;
    while (!g_quit) {
        if (next_tap < tap_points.size() &&
            SDL_GetTicks() >= started + 2000 + static_cast<Uint32>(next_tap) * 1000) {
            const auto [x, y] = tap_points[next_tap];
            static Uint32 pressed_at = 0;
            if (!g_touch_down) {
                set_touch(static_cast<float>(x), static_cast<float>(y), true);
                pressed_at = SDL_GetTicks();
            } else if (SDL_GetTicks() - pressed_at > 80) {
                set_touch(static_cast<float>(x), static_cast<float>(y), false);
                ++next_tap;
            }
        }
        if (!shot_done && SDL_GetTicks() >= shot_at) {
            save_shot(shot);
            shot_done = true;
            if (quit_after_shot != nullptr) g_quit = true;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_event(event);
        }
        uint32_t idle = 5;
        if (g_foreground) {
            std::lock_guard<std::recursive_mutex> guard(g_lvgl_mutex);
            idle = lv_timer_handler();
        }
        if (idle > 20 || !g_foreground) {
            idle = g_foreground ? 20 : 100;
        }
        SDL_WaitEventTimeout(nullptr, static_cast<int>(idle));
    }
}

void request_quit() { g_quit = true; }

}  // namespace board
