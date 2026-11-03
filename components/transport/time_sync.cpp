#include "transport/time_sync.hpp"

#include <ctime>

#include "core/clock.hpp"
#include "esp_log.h"
#include "esp_netif_sntp.h"

namespace transport {
namespace {

constexpr char kTag[] = "time";

// Czas polski z regułą zmiany na czas letni. Bez tego pasek pokazywałby UTC.
constexpr char kTimezone[] = "CET-1CEST,M3.5.0,M10.5.0/3";

// Rok, poniżej którego uznajemy, że zegar jeszcze nie ruszył.
constexpr int kFirstSaneYear = 2024;

// Po dobie pozwalamy na jedną korektę dryfu. Poza tym klient śpi.
constexpr int64_t kResyncAfterMs = 24 * 60 * 60 * 1000;

// Gdy serwer nie odpowie, odpuszczamy do następnego połączenia.
constexpr int64_t kGiveUpAfterMs = 60 * 1000;

bool g_running = false;
bool g_timezone_set = false;
int64_t g_started_at_ms = 0;
int64_t g_synced_at_ms = 0;
volatile bool g_got_time = false;

// Woła je zadanie SNTP, więc robi jak najmniej: zatrzymaniem zajmuje się UI.
void on_time_received(struct timeval*) {
    g_got_time = true;
}

void stop() {
    if (!g_running) {
        return;
    }
    esp_netif_sntp_deinit();
    g_running = false;
}

}  // namespace

void set_timezone(const char* posix) {
    setenv("TZ", posix, 1);
    tzset();
    g_timezone_set = true;
}

void start_time_sync() {
    if (g_running) {
        return;
    }
    if (time_known() && g_synced_at_ms != 0 && core::now_ms() - g_synced_at_ms < kResyncAfterMs) {
        return;
    }

    if (!g_timezone_set) {
        setenv("TZ", kTimezone, 1);
        tzset();
        g_timezone_set = true;
    }

    // Jeden serwer, bez serwera z DHCP: tamten wariant wymaga osobnego slotu
    // w konfiguracji i przy domyślnej liście kończy się błędem argumentu.
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = true;
    config.sync_cb = &on_time_received;

    g_got_time = false;
    const esp_err_t started = esp_netif_sntp_init(&config);
    if (started != ESP_OK) {
        ESP_LOGW(kTag, "nie udalo sie uruchomic synchronizacji: %s", esp_err_to_name(started));
        return;
    }
    g_running = true;
    g_started_at_ms = core::now_ms();
}

void service_time_sync() {
    if (!g_running) {
        return;
    }
    if (g_got_time) {
        g_synced_at_ms = core::now_ms();
        stop();
        ESP_LOGI(kTag, "czas pobrany, klient zatrzymany");
        return;
    }
    if (core::now_ms() - g_started_at_ms > kGiveUpAfterMs) {
        stop();
        ESP_LOGW(kTag, "brak odpowiedzi serwera czasu, sprobujemy przy nastepnym polaczeniu");
    }
}

bool time_known() {
    std::time_t now = std::time(nullptr);
    std::tm parts = {};
    localtime_r(&now, &parts);
    return parts.tm_year + 1900 >= kFirstSaneYear;
}

}  // namespace transport
