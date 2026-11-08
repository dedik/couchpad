// Composition root telefonu: te same warstwy co main/main.cpp firmware'u, tylko
// ekran, Wi-Fi i zegar dostarcza system przez SDL.

#include <SDL.h>
#include <csignal>

#include "application/app_controller.hpp"
#include "application/live_source.hpp"
#include "application/network_controller.hpp"
#include "board/board.hpp"
#include "board_phone.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "storage/settings.hpp"
#include "transport/wifi_service.hpp"
#include "ui/ui.hpp"

namespace {

constexpr char kTag[] = "pilot";

void on_restart() {
    // Firmware restartuje się po zmianie orientacji. Telefon zamyka aplikację —
    // przy następnym otwarciu wstanie w nowym układzie.
    ESP_LOGI(kTag, "restart: zamykam aplikacje");
    SDL_Quit();
}

}  // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);
    compat_set_restart_handler(&on_restart);

    // Ustawienia w prywatnym katalogu aplikacji (Android: dane aplikacji, Mac:
    // ~/Library/Application Support/probs/pilot). Na Macu można podać własny katalog.
    char* pref = SDL_GetPrefPath("probs", "pilot");
    const char* directory = argc > 1 ? argv[1] : (pref != nullptr ? pref : ".");
    compat_nvs_set_dir(directory);
    ESP_LOGI(kTag, "ustawienia w %s", directory);

    auto* live = new application::LiveSource();
    auto* controller = new application::AppController(*live);
    auto* wifi = new transport::WifiService();
    auto* network = new application::NetworkController(*wifi);

    storage::init();
    if (!board::start(board::rotation_from(storage::get(storage::Pref::Orientation)))) {
        ESP_LOGE(kTag, "ekran nie ruszyl");
        return 1;
    }
    board::set_brightness(storage::get(storage::Pref::Brightness));

    {
        board::Lock lock(0);
        ui::Services services;
        services.controller = controller;
        services.network = network;
        services.live = live;
        services.display_power = &board::set_display_on;
        services.display_brightness = &board::set_brightness;
        services.display_dim = &board::dim;
        ui::start(services);
    }

    network->start();
    controller->start();
    ESP_LOGI(kTag, "interfejs gotowy");

    board::run_loop();
    if (pref != nullptr) {
        SDL_free(pref);
    }
    SDL_Quit();
    return 0;
}
