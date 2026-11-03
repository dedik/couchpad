#include "core/memory.hpp"

#include <algorithm>
#include <cstdlib>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace core::memory {
namespace {

constexpr char kTag[] = "mem";
constexpr unsigned kKiB = 1024;

}  // namespace

void log_stage(const char* stage) {
    ESP_LOGI(kTag, "%-22s wewn. %3u kB (blok %3u kB, min %3u kB), PSRAM %5u kB", stage,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / kKiB),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / kKiB),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / kKiB),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / kKiB));
}

void log_tasks() {
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
    const UBaseType_t count = uxTaskGetNumberOfTasks() + 4;
    auto* tasks = static_cast<TaskStatus_t*>(
        heap_caps_malloc(sizeof(TaskStatus_t) * count, MALLOC_CAP_SPIRAM));
    if (tasks == nullptr) {
        return;
    }
    const UBaseType_t found = uxTaskGetSystemState(tasks, count, nullptr);
    std::sort(tasks, tasks + found, [](const TaskStatus_t& a, const TaskStatus_t& b) {
        return a.usStackHighWaterMark < b.usStackHighWaterMark;
    });
    // W ESP-IDF stos liczy się w bajtach, więc zapas to wprost bajty.
    for (UBaseType_t i = 0; i < found; ++i) {
        ESP_LOGI(kTag, "stos %-16s zapas %5u B", tasks[i].pcTaskName,
                 static_cast<unsigned>(tasks[i].usStackHighWaterMark));
    }
    heap_caps_free(tasks);
#else
    ESP_LOGI(kTag, "zapas stosów wymaga CONFIG_FREERTOS_USE_TRACE_FACILITY");
#endif
}

}  // namespace core::memory
