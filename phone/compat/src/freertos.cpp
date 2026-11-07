// FreeRTOS na std::thread: tyle, ile używa współdzielony kod firmware'u.

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <pthread.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;
const Clock::time_point g_start = Clock::now();

// Czas oczekiwania w taktach → termin. `portMAX_DELAY` znaczy „bez końca”.
bool wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, TickType_t ticks,
                const std::function<bool()>& ready) {
    if (ticks == portMAX_DELAY) {
        cv.wait(lock, ready);
        return true;
    }
    return cv.wait_for(lock, std::chrono::milliseconds(ticks), ready);
}

}  // namespace

struct compat_task {
    std::string name;
};

struct compat_queue {
    std::mutex mutex;
    std::condition_variable readable;
    std::condition_variable writable;
    std::deque<std::vector<uint8_t>> items;
    size_t length = 0;
    size_t item_size = 0;
};

struct compat_event_group {
    std::mutex mutex;
    std::condition_variable changed;
    EventBits_t bits = 0;
};

extern "C" {

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char* name, uint32_t, void* arg,
                                   UBaseType_t, TaskHandle_t* out, BaseType_t) {
    auto* task = new compat_task{name != nullptr ? name : "task"};
    std::thread([entry, arg, task] {
#if defined(__APPLE__)
        pthread_setname_np(task->name.c_str());
#elif defined(__linux__) || defined(__ANDROID__)
        pthread_setname_np(pthread_self(), task->name.substr(0, 15).c_str());
#endif
        entry(arg);
    }).detach();
    if (out != nullptr) {
        *out = task;
    }
    return pdPASS;
}

BaseType_t xTaskCreate(TaskFunction_t entry, const char* name, uint32_t stack, void* arg,
                       UBaseType_t priority, TaskHandle_t* out) {
    return xTaskCreatePinnedToCore(entry, name, stack, arg, priority, out, tskNO_AFFINITY);
}

BaseType_t xTaskCreatePinnedToCoreWithCaps(void (*entry)(void*), const char* name,
                                           uint32_t stack, void* arg, UBaseType_t priority,
                                           compat_task** out, BaseType_t core, uint32_t) {
    return xTaskCreatePinnedToCore(entry, name, stack, arg, priority, out, core);
}

void vTaskDelete(TaskHandle_t task) {
    if (task == nullptr) {
        // Zadanie kończy się samo: wątek wychodzi bez powrotu do wywołującego.
        pthread_exit(nullptr);
    }
}

void vTaskDelay(TickType_t ticks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

TickType_t xTaskGetTickCount(void) {
    return static_cast<TickType_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_start).count());
}

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size) {
    auto* queue = new compat_queue;
    queue->length = length;
    queue->item_size = item_size;
    return queue;
}

compat_queue* xQueueCreateWithCaps(UBaseType_t length, UBaseType_t item_size, uint32_t) {
    return xQueueCreate(length, item_size);
}

static BaseType_t queue_put(QueueHandle_t queue, const void* item, TickType_t wait, bool front) {
    if (queue == nullptr) {
        return pdFAIL;
    }
    std::unique_lock<std::mutex> lock(queue->mutex);
    if (!wait_until(queue->writable, lock, wait,
                    [queue] { return queue->items.size() < queue->length; })) {
        return pdFAIL;
    }
    std::vector<uint8_t> copy(queue->item_size);
    if (queue->item_size != 0 && item != nullptr) {
        std::memcpy(copy.data(), item, queue->item_size);
    }
    if (front) {
        queue->items.push_front(std::move(copy));
    } else {
        queue->items.push_back(std::move(copy));
    }
    lock.unlock();
    queue->readable.notify_one();
    return pdPASS;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait) {
    return queue_put(queue, item, wait, false);
}

BaseType_t xQueueSendToFront(QueueHandle_t queue, const void* item, TickType_t wait) {
    return queue_put(queue, item, wait, true);
}

static BaseType_t queue_get(QueueHandle_t queue, void* out, TickType_t wait, bool remove) {
    if (queue == nullptr) {
        return pdFAIL;
    }
    std::unique_lock<std::mutex> lock(queue->mutex);
    if (!wait_until(queue->readable, lock, wait, [queue] { return !queue->items.empty(); })) {
        return pdFAIL;
    }
    if (queue->item_size != 0 && out != nullptr) {
        std::memcpy(out, queue->items.front().data(), queue->item_size);
    }
    if (remove) {
        queue->items.pop_front();
        lock.unlock();
        queue->writable.notify_one();
    }
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* out, TickType_t wait) {
    return queue_get(queue, out, wait, true);
}

BaseType_t xQueuePeek(QueueHandle_t queue, void* out, TickType_t wait) {
    return queue_get(queue, out, wait, false);
}

BaseType_t xQueueReset(QueueHandle_t queue) {
    if (queue == nullptr) {
        return pdFAIL;
    }
    {
        std::lock_guard<std::mutex> lock(queue->mutex);
        queue->items.clear();
    }
    queue->writable.notify_all();
    return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue) {
    if (queue == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(queue->mutex);
    return static_cast<UBaseType_t>(queue->items.size());
}

void vQueueDelete(QueueHandle_t queue) { delete queue; }

SemaphoreHandle_t xSemaphoreCreateBinary(void) { return xQueueCreate(1, 0); }

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t semaphore = xQueueCreate(1, 0);
    xQueueSend(semaphore, nullptr, 0);
    return semaphore;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t wait) {
    return xQueueReceive(semaphore, nullptr, wait);
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    return xQueueSend(semaphore, nullptr, 0);
}

EventGroupHandle_t xEventGroupCreate(void) { return new compat_event_group; }

EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits) {
    std::lock_guard<std::mutex> lock(group->mutex);
    group->bits |= bits;
    group->changed.notify_all();
    return group->bits;
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits) {
    std::lock_guard<std::mutex> lock(group->mutex);
    const EventBits_t before = group->bits;
    group->bits &= ~bits;
    return before;
}

EventBits_t xEventGroupGetBits(EventGroupHandle_t group) {
    std::lock_guard<std::mutex> lock(group->mutex);
    return group->bits;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bits, BaseType_t clear,
                                BaseType_t all, TickType_t wait) {
    std::unique_lock<std::mutex> lock(group->mutex);
    auto ready = [&] {
        return all ? (group->bits & bits) == bits : (group->bits & bits) != 0;
    };
    wait_until(group->changed, lock, wait, ready);
    const EventBits_t result = group->bits;
    if (ready() && clear) {
        group->bits &= ~bits;
    }
    return result;
}

}  // extern "C"
