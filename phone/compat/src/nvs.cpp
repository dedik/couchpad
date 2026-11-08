// NVS jako plik tekstowy `nvs.txt` w prywatnym katalogu aplikacji.
//
// Format jednej linii: `<przestrzeń>\t<klucz>\t<typ>\t<wartość>`, wartości tekstowe
// z ucieczką `\\`, `\t`, `\n`. Zapis atomowy przez plik tymczasowy i rename.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

#include "esp_log.h"
#include "nvs_flash.h"

namespace {

constexpr char kTag[] = "nvs";

struct Entry {
    char type;  // 's' tekst, 'u' liczba bez znaku
    std::string value;
};

using Space = std::map<std::string, Entry>;

std::mutex g_mutex;
std::string g_dir = ".";
bool g_loaded = false;
std::map<std::string, Space> g_spaces;
std::map<nvs_handle_t, std::string> g_handles;
nvs_handle_t g_next = 1;

std::string path() { return g_dir + "/nvs.txt"; }

std::string escape(const std::string& in) {
    std::string out;
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            default: out += c;
        }
    }
    return out;
}

std::string unescape(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 1 < in.size()) {
            ++i;
            out += in[i] == 't' ? '\t' : in[i] == 'n' ? '\n' : in[i];
        } else {
            out += in[i];
        }
    }
    return out;
}

void load() {
    g_spaces.clear();
    std::ifstream file(path());
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::string space, key, type, value;
        if (!std::getline(fields, space, '\t') || !std::getline(fields, key, '\t') ||
            !std::getline(fields, type, '\t')) {
            continue;
        }
        std::getline(fields, value);
        g_spaces[unescape(space)][unescape(key)] = Entry{type.empty() ? 's' : type[0],
                                                        unescape(value)};
    }
    g_loaded = true;
}

bool save() {
    const std::string target = path();
    const std::string temporary = target + ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        if (!file) {
            ESP_LOGE(kTag, "nie mozna zapisac %s", temporary.c_str());
            return false;
        }
        for (const auto& [space, entries] : g_spaces) {
            for (const auto& [key, entry] : entries) {
                file << escape(space) << '\t' << escape(key) << '\t' << entry.type << '\t'
                     << escape(entry.value) << '\n';
            }
        }
    }
    return std::rename(temporary.c_str(), target.c_str()) == 0;
}

Space* space_of(nvs_handle_t handle) {
    auto it = g_handles.find(handle);
    return it == g_handles.end() ? nullptr : &g_spaces[it->second];
}

template <typename T>
esp_err_t get_number(nvs_handle_t handle, const char* key, T* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Space* space = space_of(handle);
    if (space == nullptr) return ESP_ERR_NVS_NOT_INITIALIZED;
    auto it = space->find(key);
    if (it == space->end()) return ESP_ERR_NVS_NOT_FOUND;
    if (it->second.type != 'u') return ESP_ERR_NVS_TYPE_MISMATCH;
    *out = static_cast<T>(std::stoull(it->second.value));
    return ESP_OK;
}

template <typename T>
esp_err_t set_number(nvs_handle_t handle, const char* key, T value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Space* space = space_of(handle);
    if (space == nullptr) return ESP_ERR_NVS_NOT_INITIALIZED;
    (*space)[key] = Entry{'u', std::to_string(static_cast<unsigned long long>(value))};
    return ESP_OK;
}

}  // namespace

extern "C" {

void compat_nvs_set_dir(const char* directory) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_dir = directory != nullptr ? directory : ".";
    g_loaded = false;
}

esp_err_t nvs_flash_init(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_loaded) {
        load();
    }
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_spaces.clear();
    return save() ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_open(const char* name_space, nvs_open_mode_t, nvs_handle_t* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_loaded) {
        load();
    }
    const nvs_handle_t handle = g_next++;
    g_handles[handle] = name_space;
    *out = handle;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_handles.erase(handle);
}

esp_err_t nvs_commit(nvs_handle_t) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return save() ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Space* space = space_of(handle);
    if (space == nullptr) return ESP_ERR_NVS_NOT_INITIALIZED;
    return space->erase(key) != 0 ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_erase_all(nvs_handle_t handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Space* space = space_of(handle);
    if (space == nullptr) return ESP_ERR_NVS_NOT_INITIALIZED;
    space->clear();
    return ESP_OK;
}

esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out, size_t* length) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Space* space = space_of(handle);
    if (space == nullptr) return ESP_ERR_NVS_NOT_INITIALIZED;
    auto it = space->find(key);
    if (it == space->end()) return ESP_ERR_NVS_NOT_FOUND;
    if (it->second.type != 's') return ESP_ERR_NVS_TYPE_MISMATCH;
    const size_t needed = it->second.value.size() + 1;
    if (out == nullptr) {
        *length = needed;
        return ESP_OK;
    }
    if (*length < needed) {
        *length = needed;
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    std::memcpy(out, it->second.value.c_str(), needed);
    *length = needed;
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Space* space = space_of(handle);
    if (space == nullptr) return ESP_ERR_NVS_NOT_INITIALIZED;
    (*space)[key] = Entry{'s', value != nullptr ? value : ""};
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char* k, uint8_t* out) { return get_number(h, k, out); }
esp_err_t nvs_set_u8(nvs_handle_t h, const char* k, uint8_t v) { return set_number(h, k, v); }
esp_err_t nvs_get_u16(nvs_handle_t h, const char* k, uint16_t* out) { return get_number(h, k, out); }
esp_err_t nvs_set_u16(nvs_handle_t h, const char* k, uint16_t v) { return set_number(h, k, v); }
esp_err_t nvs_get_u32(nvs_handle_t h, const char* k, uint32_t* out) { return get_number(h, k, out); }
esp_err_t nvs_set_u32(nvs_handle_t h, const char* k, uint32_t v) { return set_number(h, k, v); }

}  // extern "C"
