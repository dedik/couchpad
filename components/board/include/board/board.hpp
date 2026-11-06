#pragma once

// Start ekranu i dotyku na zweryfikowanych wartościach tej płytki.
// Piny, timingi i wersje: docs/hardware/verified-board-map.md.
//
// Komponent jest jedyną granicą między aplikacją a BSP producenta. Jeśli kiedyś
// zmieni się płytka, zmienia się ten plik, a nie ekrany.

#include <cstdint>

namespace board {

// Natywna rozdzielczość panelu w pionie. Makieta Pulpitu jest projektowana
// dokładnie w tych pikselach, bez skalowania. To wymiary bufora ramki DPI —
// po obrocie interfejs widzi je zamienione (`screen_width()`, `screen_height()`).
inline constexpr int32_t kScreenWidth = 480;
inline constexpr int32_t kScreenHeight = 800;

// Orientacja interfejsu. Panel DPI nie obraca obrazu sam (brak swap_xy), więc
// poziom to obrót w esp_lvgl_port przy każdym odświeżeniu, procesorem. PPA jest
// wyłączone: zawieszało zadanie LVGL przy przewijaniu (ADR 0013). Dotyk GT911 przelicza LVGL
// według obrotu ekranu. Wartości są zapisane w NVS (storage::Pref::Orientation).
enum class Rotation : uint8_t {
    Portrait = 0,      // natywnie, 480 x 800
    Landscape90 = 1,   // 800 x 480, obrót o 90°
    Landscape270 = 2,  // 800 x 480, obrót o 270° (pilot odwrócony)
};

// Wartość z NVS albo spoza zakresu daje pion.
Rotation rotation_from(uint8_t stored);

// Uruchamia MIPI-DSI, ST7701, GT911 i zadanie LVGL. Zwraca false, gdy ekran nie ruszył.
// Orientację ustala się tylko tutaj: ekrany budowane są raz, pod jeden rozmiar.
bool start(Rotation rotation = Rotation::Portrait);

// Orientacja i logiczny rozmiar ekranu po obrocie (to, co widzi LVGL).
Rotation rotation();
int32_t screen_width();
int32_t screen_height();

// Jasność podświetlenia w procentach (LEDC na GPIO 23).
// Ustawia i zapamiętuje jasność; `set_display_on(true)` wraca do tej wartości.
void set_brightness(int percent);
// Chwilowe przyciemnienie bez zmiany zapamiętanej jasności.
void dim(int percent);

// Gasi i zapala podświetlenie. Sam panel pracuje dalej — wybudzenie jest
// natychmiastowe, bo nie trzeba go inicjalizować od nowa.
void set_display_on(bool on);

// Konsola USB-Serial-JTAG na czas rozwoju (CONFIG_PILOT_USB_CONSOLE): zrzut bieżącej
// ramki panelu i zdalny dotyk, sterowane z tools/remote.py.
void start_usb_console();

// Mutex zadania LVGL. Każde wywołanie lv_* musi być w jego zakresie —
// wymaga tego ADR 0003 i sam esp_lvgl_port.
bool lock(uint32_t timeout_ms);
void unlock();

// RAII na powyższe. Używać wszędzie tam, gdzie kod poza zadaniem UI dotyka LVGL.
class Lock {
public:
    explicit Lock(uint32_t timeout_ms = 0) : held_(lock(timeout_ms)) {}
    ~Lock() { if (held_) unlock(); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    bool held() const { return held_; }

private:
    bool held_;
};

}  // namespace board
