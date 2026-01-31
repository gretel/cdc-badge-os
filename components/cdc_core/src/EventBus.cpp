#include "cdc_core/EventBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cdc_log.h"
#include "esp_timer.h"

static const char* TAG = "EventBus";

namespace cdc::core {

EventBus& EventBus::instance() {
    static EventBus instance;
    return instance;
}

bool EventBus::init(size_t queueSize) {
    if (initialized_) {
        LOG_W(TAG, "Already initialized");
        return true;
    }

    queue_ = xQueueCreate(queueSize, sizeof(Event));
    if (!queue_) {
        LOG_E(TAG, "Failed to create event queue");
        return false;
    }

    initialized_ = true;
    LOG_I(TAG, "Initialized with queue size %u", queueSize);
    return true;
}

uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    if (!handler) return 0;

    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {
            handlers_[i].handler = handler;
            handlers_[i].mask = mask;
            handlers_[i].active = true;
            LOG_D(TAG, "Handler %u subscribed (mask: 0x%08lx)", i + 1, mask);
            return i + 1;  // Return 1-based ID
        }
    }

    LOG_E(TAG, "No free handler slots");
    return 0;
}

void EventBus::unsubscribe(uint8_t id) {
    if (id == 0 || id > MAX_HANDLERS) return;

    handlers_[id - 1].active = false;
    handlers_[id - 1].handler = nullptr;
    LOG_D(TAG, "Handler %u unsubscribed", id);
}

bool EventBus::publish(const Event& event, bool fromISR) {
    if (!initialized_ || !queue_) return false;

    BaseType_t result;
    if (fromISR) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        result = xQueueSendFromISR(static_cast<QueueHandle_t>(queue_),
                                    &event, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    } else {
        result = xQueueSend(static_cast<QueueHandle_t>(queue_),
                            &event, pdMS_TO_TICKS(10));
    }

    return result == pdTRUE;
}

bool EventBus::publish(EventType type, uint8_t value) {
    Event event = {};
    event.type = type;
    event.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    event.data.value = value;
    return publish(event);
}

void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        // Dispatch to all matching handlers
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                // Check mask (0 = receive all)
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);
                }
            }
        }
    }
}

} // namespace cdc::core
