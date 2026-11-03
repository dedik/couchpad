#include "core/clock.hpp"

#include "esp_timer.h"

namespace core {

int64_t now_ms() {
    return esp_timer_get_time() / 1000;
}

}  // namespace core
