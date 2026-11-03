#include "storage/settings.hpp"

#include <atomic>
#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace storage {
namespace {

constexpr char kTag[] = "storage";
constexpr char kNamespace[] = "pilot";
constexpr char kKeySsid[] = "wifi_ssid";
constexpr char kKeyPassword[] = "wifi_pass";
constexpr char kKeyMqttHost[] = "mq_host";
constexpr char kKeyMqttPort[] = "mq_port";
constexpr char kKeyMqttUser[] = "mq_user";
constexpr char kKeyMqttPass[] = "mq_pass";
constexpr char kKeyZ906Id[] = "z906_id";
constexpr char kKeyRemoteId[] = "remote_id";
constexpr char kKeyTvPrefix[] = "tv_prefix";
// Domyślny prefiks Glasshouse. To ustawienie fabryczne aplikacji na TV, nie
// adres domowej infrastruktury.
constexpr char kDefaultTvPrefix[] = "lgtv";
constexpr char kKeyRoom[] = "room";

// Klucz NVS i wartość domyślna każdej preferencji, w kolejności `Pref`.
// Nowe ustawienie ekranu WIĘCEJ to nowy wiersz tutaj i w `Pref`.
struct PrefSpec {
    const char* key;
    uint8_t fallback;
};

constexpr PrefSpec kPrefs[] = {
    {"kb_qwerty", 0},  // Pref::KeyboardQwerty
    {"kb_live", 1},    // Pref::KeyboardLive
    // Domyślne wartości odtwarzają zachowanie sprzed ekranu ustawień.
    {"bright", 100},   // Pref::Brightness
    {"sleep_idx", 1},  // Pref::ScreenTimeout — 30 s
    {"dim", 0},        // Pref::DimBeforeSleep
    {"vol_max", 100},  // Pref::VolumeLimit
    {"night", 0},      // Pref::NightMode — wyłączony
    {"tz", 0},         // Pref::TimeZone — Polska
    {"pad_step", 26},  // Pref::TrackpadStep
    {"kb_auto", 1},    // Pref::KeyboardAutoOpen
    {"theme", 0},      // Pref::ThemeMode — ciemny, jak przed jasnym stylem
    {"theme_hrs", 1},  // Pref::ThemeHours — ciemny od 18 do 8
};
static_assert(sizeof(kPrefs) / sizeof(kPrefs[0]) == static_cast<std::size_t>(Pref::Count),
              "kazda preferencja potrzebuje klucza i wartosci domyslnej");

// Preferencje czyta się przy rysowaniu ekranów, a otwarcie NVS kosztuje.
// Trzymamy kopię każdej wartości przesuniętą o jeden, żeby zero (stan
// początkowy zmiennej statycznej) znaczyło „jeszcze nie odczytana”.
std::atomic<uint16_t> g_cache[static_cast<std::size_t>(Pref::Count)];

void forget_cache() {
    for (auto& slot : g_cache) {
        slot = 0;
    }
}

// Otwarta przestrzeń „pilot” w NVS, zamykana przy wyjściu z zakresu.
class Nvs {
public:
    explicit Nvs(nvs_open_mode_t mode) { ok_ = nvs_open(kNamespace, mode, &handle_) == ESP_OK; }
    ~Nvs() {
        if (ok_) {
            nvs_close(handle_);
        }
    }
    Nvs(const Nvs&) = delete;
    Nvs& operator=(const Nvs&) = delete;

    bool ok() const { return ok_; }

    // `false`, gdy klucza nie ma; bufor zostaje wtedy pusty.
    bool get_text(const char* key, char* buffer, std::size_t size) const {
        std::size_t length = size;
        return ok_ && nvs_get_str(handle_, key, buffer, &length) == ESP_OK;
    }
    template <std::size_t N>
    bool get_text(const char* key, core::Text<N>& out) const {
        char buffer[N] = {};
        const bool found = get_text(key, buffer, sizeof(buffer));
        out.assign(buffer);
        return found;
    }
    uint8_t get_u8(const char* key, uint8_t fallback) const {
        uint8_t value = fallback;
        if (ok_) {
            nvs_get_u8(handle_, key, &value);
        }
        return value;
    }
    uint16_t get_u16(const char* key, uint16_t fallback) const {
        uint16_t value = fallback;
        if (ok_) {
            nvs_get_u16(handle_, key, &value);
        }
        return value;
    }

    bool set_text(const char* key, const char* value) {
        return ok_ && nvs_set_str(handle_, key, value) == ESP_OK;
    }
    bool set_u8(const char* key, uint8_t value) {
        return ok_ && nvs_set_u8(handle_, key, value) == ESP_OK;
    }
    bool set_u16(const char* key, uint16_t value) {
        return ok_ && nvs_set_u16(handle_, key, value) == ESP_OK;
    }
    void erase(const char* key) {
        if (ok_) {
            nvs_erase_key(handle_, key);
        }
    }
    bool erase_all() { return ok_ && nvs_erase_all(handle_) == ESP_OK; }
    bool commit() { return ok_ && nvs_commit(handle_) == ESP_OK; }

private:
    nvs_handle_t handle_ = 0;
    bool ok_ = false;
};

}  // namespace

void init() {
    // Wołają ją main i NetworkController; druga i kolejne nic nie robią.
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    esp_err_t ready = nvs_flash_init();
    if (ready == ESP_ERR_NVS_NO_FREE_PAGES || ready == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ready = nvs_flash_init();
    }
    if (ready != ESP_OK) {
        ESP_LOGE(kTag, "NVS niedostepne: %s", esp_err_to_name(ready));
    }
}

bool load_wifi(WifiCredentials& out) {
    Nvs nvs(NVS_READONLY);
    if (!nvs.ok()) {
        return false;
    }
    const bool has_ssid = nvs.get_text(kKeySsid, out.ssid);
    nvs.get_text(kKeyPassword, out.password);
    return has_ssid && !out.ssid.empty();
}

bool save_wifi(const WifiCredentials& credentials) {
    Nvs nvs(NVS_READWRITE);
    bool ok = nvs.set_text(kKeySsid, credentials.ssid.c_str());
    ok = ok && nvs.set_text(kKeyPassword, credentials.password.c_str());
    ok = ok && nvs.commit();
    // Nazwa sieci jest prywatna — do logu trafia wyłącznie fakt zapisu.
    ESP_LOGI(kTag, "dane dostepowe %s", ok ? "zapisane" : "niezapisane");
    return ok;
}

void clear_wifi() {
    Nvs nvs(NVS_READWRITE);
    nvs.erase(kKeySsid);
    nvs.erase(kKeyPassword);
    nvs.commit();
    ESP_LOGI(kTag, "dane dostepowe usuniete");
}

bool load_mqtt(MqttSettings& out) {
    Nvs nvs(NVS_READONLY);
    if (!nvs.ok()) {
        return false;
    }
    const bool has_host = nvs.get_text(kKeyMqttHost, out.host);
    nvs.get_text(kKeyMqttUser, out.username);
    nvs.get_text(kKeyMqttPass, out.password);
    nvs.get_text(kKeyZ906Id, out.z906_id);
    nvs.get_text(kKeyRemoteId, out.remote_id);
    // Brak klucza to pilot sprzed ADR 0008: dostaje prefiks domyślny. Pusty
    // zapisany świadomie zostaje pusty i wyłącza telewizor.
    if (!nvs.get_text(kKeyTvPrefix, out.tv_prefix)) {
        out.tv_prefix.assign(kDefaultTvPrefix);
    }
    const uint16_t port = nvs.get_u16(kKeyMqttPort, 1883);
    out.port = port != 0 ? port : 1883;
    return has_host && !out.host.empty();
}

bool save_mqtt(const MqttSettings& settings) {
    Nvs nvs(NVS_READWRITE);
    bool ok = nvs.set_text(kKeyMqttHost, settings.host.c_str());
    ok = ok && nvs.set_u16(kKeyMqttPort, settings.port);
    ok = ok && nvs.set_text(kKeyMqttUser, settings.username.c_str());
    ok = ok && nvs.set_text(kKeyMqttPass, settings.password.c_str());
    ok = ok && nvs.set_text(kKeyZ906Id, settings.z906_id.c_str());
    ok = ok && nvs.set_text(kKeyRemoteId, settings.remote_id.c_str());
    ok = ok && nvs.set_text(kKeyTvPrefix, settings.tv_prefix.c_str());
    ok = ok && nvs.commit();
    // Adres brokera jest adresem domowej infrastruktury — do logu idzie sam wynik.
    ESP_LOGI(kTag, "ustawienia brokera %s", ok ? "zapisane" : "niezapisane");
    return ok;
}

void load_room(RoomName& out) {
    Nvs nvs(NVS_READONLY);
    if (!nvs.get_text(kKeyRoom, out) || out.empty()) {
        out.assign("SALON");
    }
}

void save_room(const RoomName& room) {
    Nvs nvs(NVS_READWRITE);
    nvs.set_text(kKeyRoom, room.c_str());
    nvs.commit();
}

uint8_t get(Pref pref) {
    const auto index = static_cast<std::size_t>(pref);
    const uint16_t cached = g_cache[index].load();
    if (cached != 0) {
        return static_cast<uint8_t>(cached - 1);
    }
    const uint8_t value = Nvs(NVS_READONLY).get_u8(kPrefs[index].key, kPrefs[index].fallback);
    g_cache[index] = value + 1;
    return value;
}

void set(Pref pref, uint8_t value) {
    const auto index = static_cast<std::size_t>(pref);
    g_cache[index] = value + 1;
    Nvs nvs(NVS_READWRITE);
    if (!(nvs.set_u8(kPrefs[index].key, value) && nvs.commit())) {
        ESP_LOGW(kTag, "nie zapisano preferencji %s", kPrefs[index].key);
    }
}

bool factory_reset() {
    Nvs nvs(NVS_READWRITE);
    const bool ok = nvs.erase_all() && nvs.commit();
    forget_cache();
    ESP_LOGI(kTag, "ustawienia fabryczne %s", ok ? "przywrocone" : "nieprzywrocone");
    return ok;
}

}  // namespace storage
