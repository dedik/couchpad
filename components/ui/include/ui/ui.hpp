#pragma once

// Wejście do interfejsu. Wszystkie wywołania LVGL dzieją się w zadaniu UI (ADR 0003),
// więc `start()` wolno zawołać wyłącznie spod blokady `board::Lock`.

#include "application/app_controller.hpp"
#include "application/live_source.hpp"
#include "application/network_controller.hpp"

namespace ui {

// Podświetlenie należy do płytki, a interfejs tylko wie, kiedy je zgasić.
// Dzięki temu `ui` nie zależy od `board`.
using DisplayPower = void (*)(bool on);
// Jasność w procentach: ustawiona na stałe albo chwilowe przyciemnienie.
using DisplayLevel = void (*)(int percent);

// Wszystko, czego interfejs potrzebuje od reszty programu.
struct Services {
    application::AppController* controller = nullptr;
    application::NetworkController* network = nullptr;
    application::LiveSource* live = nullptr;
    DisplayPower display_power = nullptr;
    DisplayLevel display_brightness = nullptr;
    DisplayLevel display_dim = nullptr;
};

void start(const Services& services);

}  // namespace ui
