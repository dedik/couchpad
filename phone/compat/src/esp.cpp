// Drobne usługi ESP-IDF: log, esp_timer, zdarzenia IP, adres sieciowy, opis aplikacji.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef PILOT_PHONE_VERSION
#define PILOT_PHONE_VERSION "phone-dev"
#endif

namespace {

using Clock = std::chrono::steady_clock;
const Clock::time_point g_start = Clock::now();

}  // namespace

// ---------------------------------------------------------------- log

extern "C" void compat_log(esp_log_level_t level, const char* tag, const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
#ifdef __ANDROID__
    int priority = ANDROID_LOG_INFO;
    switch (level) {
        case ESP_LOG_ERROR: priority = ANDROID_LOG_ERROR; break;
        case ESP_LOG_WARN: priority = ANDROID_LOG_WARN; break;
        case ESP_LOG_DEBUG: priority = ANDROID_LOG_DEBUG; break;
        case ESP_LOG_VERBOSE: priority = ANDROID_LOG_VERBOSE; break;
        default: break;
    }
    __android_log_print(priority, "pilot", "%s: %s", tag, message);
#else
    static const char letters[] = "NEWIDV";
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_start)
                        .count();
    std::fprintf(stderr, "%c (%lld) %s: %s\n", letters[level], static_cast<long long>(ms), tag,
                 message);
#endif
}

extern "C" const char* esp_err_to_name(esp_err_t code) {
    switch (code) {
        case ESP_OK: return "ESP_OK";
        case ESP_FAIL: return "ESP_FAIL";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
        case ESP_ERR_NVS_NOT_FOUND: return "ESP_ERR_NVS_NOT_FOUND";
        default: return "ESP_ERR";
    }
}

// ---------------------------------------------------------------- esp_timer

struct compat_timer {
    esp_timer_create_args_t args{};
    Clock::time_point due{};
    std::chrono::microseconds period{0};
    bool active = false;
    uint64_t generation = 0;
};

namespace {

// Jeden wątek obsługuje wszystkie timery, jak zadanie esp_timer.
class TimerService {
public:
    static TimerService& get() {
        static TimerService* service = new TimerService;
        return *service;
    }

    void arm(compat_timer* timer, uint64_t delay_us, uint64_t period_us) {
        std::lock_guard<std::mutex> lock(mutex_);
        timer->due = Clock::now() + std::chrono::microseconds(delay_us);
        timer->period = std::chrono::microseconds(period_us);
        timer->active = true;
        ++timer->generation;
        if (std::find(timers_.begin(), timers_.end(), timer) == timers_.end()) {
            timers_.push_back(timer);
        }
        changed_.notify_one();
    }

    void disarm(compat_timer* timer) {
        std::lock_guard<std::mutex> lock(mutex_);
        timer->active = false;
        ++timer->generation;
        changed_.notify_one();
    }

    void forget(compat_timer* timer) {
        std::lock_guard<std::mutex> lock(mutex_);
        timer->active = false;
        timers_.erase(std::remove(timers_.begin(), timers_.end(), timer), timers_.end());
    }

    bool active(compat_timer* timer) {
        std::lock_guard<std::mutex> lock(mutex_);
        return timer->active;
    }

private:
    TimerService() { std::thread([this] { run(); }).detach(); }

    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (true) {
            compat_timer* next = nullptr;
            for (auto* timer : timers_) {
                if (timer->active && (next == nullptr || timer->due < next->due)) {
                    next = timer;
                }
            }
            if (next == nullptr) {
                changed_.wait(lock);
                continue;
            }
            const uint64_t generation = next->generation;
            if (changed_.wait_until(lock, next->due) != std::cv_status::timeout) {
                continue;
            }
            if (!next->active || next->generation != generation || Clock::now() < next->due) {
                continue;
            }
            if (next->period.count() > 0) {
                next->due += next->period;
            } else {
                next->active = false;
            }
            const esp_timer_create_args_t args = next->args;
            lock.unlock();
            args.callback(args.arg);
            lock.lock();
        }
    }

    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<compat_timer*> timers_;
};

}  // namespace

extern "C" int64_t esp_timer_get_time(void) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - g_start).count();
}

extern "C" esp_err_t esp_timer_create(const esp_timer_create_args_t* args,
                                      esp_timer_handle_t* out) {
    if (args == nullptr || args->callback == nullptr || out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    auto* timer = new compat_timer;
    timer->args = *args;
    *out = timer;
    return ESP_OK;
}

extern "C" esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    if (TimerService::get().active(timer)) return ESP_ERR_INVALID_STATE;
    TimerService::get().arm(timer, timeout_us, 0);
    return ESP_OK;
}

extern "C" esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    if (TimerService::get().active(timer)) return ESP_ERR_INVALID_STATE;
    TimerService::get().arm(timer, period_us, period_us);
    return ESP_OK;
}

extern "C" esp_err_t esp_timer_stop(esp_timer_handle_t timer) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    if (!TimerService::get().active(timer)) return ESP_ERR_INVALID_STATE;
    TimerService::get().disarm(timer);
    return ESP_OK;
}

extern "C" esp_err_t esp_timer_delete(esp_timer_handle_t timer) {
    if (timer == nullptr) return ESP_ERR_INVALID_ARG;
    TimerService::get().forget(timer);
    delete timer;
    return ESP_OK;
}

extern "C" bool esp_timer_is_active(esp_timer_handle_t timer) {
    return timer != nullptr && TimerService::get().active(timer);
}

// ---------------------------------------------------------------- random, restart, opis

extern "C" uint32_t esp_random(void) {
    static std::mutex mutex;
    static std::random_device device;
    static std::mt19937 engine(device());
    std::lock_guard<std::mutex> lock(mutex);
    return engine();
}

namespace {
compat_restart_handler_t g_restart = nullptr;
}

extern "C" void compat_set_restart_handler(compat_restart_handler_t handler) {
    g_restart = handler;
}

extern "C" void esp_restart(void) {
    if (g_restart != nullptr) {
        g_restart();
    }
    std::exit(0);
}

extern "C" const esp_app_desc_t* esp_app_get_description(void) {
    static esp_app_desc_t desc = [] {
        esp_app_desc_t value{};
        std::snprintf(value.version, sizeof(value.version), "%s", PILOT_PHONE_VERSION);
        std::snprintf(value.project_name, sizeof(value.project_name), "%s", "pilot-phone");
        std::snprintf(value.time, sizeof(value.time), "%s", __TIME__);
        std::snprintf(value.date, sizeof(value.date), "%s", __DATE__);
        std::snprintf(value.idf_ver, sizeof(value.idf_ver), "%s", "compat");
        return value;
    }();
    return &desc;
}

// ---------------------------------------------------------------- zdarzenia IP

const esp_event_base_t IP_EVENT = "IP_EVENT";

namespace {

struct Registration {
    esp_event_base_t base;
    int32_t id;
    esp_event_handler_t handler;
    void* arg;
};

std::mutex g_events_mutex;
std::vector<Registration*> g_registrations;

}  // namespace

extern "C" esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
                                                         esp_event_handler_t handler, void* arg,
                                                         esp_event_handler_instance_t* instance) {
    auto* registration = new Registration{base, id, handler, arg};
    std::lock_guard<std::mutex> lock(g_events_mutex);
    g_registrations.push_back(registration);
    if (instance != nullptr) {
        *instance = registration;
    }
    return ESP_OK;
}

extern "C" esp_err_t esp_event_handler_instance_unregister(esp_event_base_t, int32_t,
                                                           esp_event_handler_instance_t instance) {
    std::lock_guard<std::mutex> lock(g_events_mutex);
    auto* registration = static_cast<Registration*>(instance);
    auto it = std::find(g_registrations.begin(), g_registrations.end(), registration);
    if (it == g_registrations.end()) {
        return ESP_ERR_NOT_FOUND;
    }
    g_registrations.erase(it);
    delete registration;
    return ESP_OK;
}

extern "C" esp_err_t esp_event_post(esp_event_base_t base, int32_t id, const void* data, size_t,
                                    uint32_t) {
    std::vector<Registration> targets;
    {
        std::lock_guard<std::mutex> lock(g_events_mutex);
        for (auto* registration : g_registrations) {
            if (registration->base == base &&
                (registration->id == id || registration->id == ESP_EVENT_ANY_ID)) {
                targets.push_back(*registration);
            }
        }
    }
    for (const auto& target : targets) {
        target.handler(target.arg, base, id, const_cast<void*>(data));
    }
    return ESP_OK;
}

// ---------------------------------------------------------------- adres sieciowy

struct compat_netif {
    int unused;
};

extern "C" esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) {
    static compat_netif station{};
    return &station;
}

// Lokalny adres trasy do sieci zewnętrznej: `connect` na gnieździe UDP nie wysyła
// żadnego pakietu, a system wybiera interfejs i adres źródłowy. Działa na każdej
// wersji Androida (getifaddrs dopiero od API 24).
extern "C" esp_err_t esp_netif_get_ip_info(esp_netif_t*, esp_netif_ip_info_t* info) {
    if (info == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::memset(info, 0, sizeof(*info));
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return ESP_FAIL;
    }
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "192.0.2.1", &remote.sin_addr);  // TEST-NET-1, nigdy nie routowany dalej
    esp_err_t result = ESP_FAIL;
    if (connect(fd, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
        sockaddr_in local{};
        socklen_t length = sizeof(local);
        if (getsockname(fd, reinterpret_cast<sockaddr*>(&local), &length) == 0) {
            info->ip.addr = local.sin_addr.s_addr;
            result = ESP_OK;
        }
    }
    close(fd);
    return result;
}
