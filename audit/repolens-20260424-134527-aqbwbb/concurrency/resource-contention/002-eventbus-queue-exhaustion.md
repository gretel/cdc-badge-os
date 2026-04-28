---
title: "[MEDIUM] EventBus Queue Exhaustion Under High Event Load"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

The `EventBus` uses a fixed-size FreeRTOS queue (default 32 events) with **no backpressure mechanism**. When event producers (ISR handlers, tasks) generate events faster than the main loop can consume them, the queue fills up and events are silently dropped.

**Location**: `components/cdc_core/src/EventBus.cpp:24-100`

## Impact

**Resource Contention Risk**: Under high load conditions (rapid key presses, BLE notifications, USB data, periodic ticks), the event queue can overflow, causing:

1. **Lost events**: Critical events (lock/unlock, battery warnings) may be dropped
2. **Silent failures**: `publish()` returns `false` but callers often don't check return value
3. **UI lag**: Event processing happens in main loop; queue buildup delays all event handling

**Evidence**:
- `EventBus.h:76`: `static constexpr size_t DEFAULT_QUEUE_SIZE = 32;` - hardcoded small queue
- `EventBus.cpp:84-98`: `publish()` uses 10ms timeout but doesn't block indefinitely
- `EventBus.cpp:121-139`: `process()` drains queue but calls handler synchronously (can be slow)

## Evidence

**Queue Initialization** (`components/cdc_core/src/EventBus.cpp:24-38`):
```cpp
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
```

**Publish with Timeout** (`components/cdc_core/src/EventBus.cpp:84-98`):
```cpp
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
                            &event, pdMS_TO_TICKS(10));  // 10ms timeout
    }

    return result == pdTRUE;  // Returns false if queue full
}
```

**Event Processing** (`components/cdc_core/src/EventBus.cpp:121-139`):
```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        // Dispatch to all matching handlers
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);  // <-- Can be slow!
                }
            }
        }
    }
}
```

**Main Loop Usage** (`main/main.cpp:241-250`):
```cpp
while (true) {
    EventBus::instance().process();  // Only processes here
    cdc::serial::SerialCmd::process();

    // ... other processing ...

    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay per iteration
}
```

With 10ms delay, if event handlers take >10ms total, the queue can fill up (32 events max = 320ms buffer).

## Recommended Fix

1. **Increase queue size** and make it configurable:
```cpp
// In EventBus.h
static constexpr size_t DEFAULT_QUEUE_SIZE = 64;  // Double it
static constexpr size_t MAX_QUEUE_SIZE = 128;     // Cap to prevent memory exhaustion
```

2. **Add queue statistics monitoring**:
```cpp
// In EventBus.h
uint32_t getQueueDepth() const;
uint32_t getPeakDepth() const;
bool isNearCapacity(float threshold);  // e.g., 80% full

// In EventBus.cpp
uint32_t EventBus::getQueueDepth() const {
    return uxQueueMessagesWaiting(static_cast<QueueHandle_t>(queue_));
}
```

3. **Add backpressure logging** when queue is near capacity:
```cpp
bool EventBus::publish(const Event& event, bool fromISR) {
    // ... existing code ...

    // Check queue depth before sending
    uint32_t depth = uxQueueMessagesWaiting(static_cast<QueueHandle_t>(queue_));
    if (depth > 24) {  // 75% of 32
        LOG_W(TAG, "Event queue at %lu/%lu events", depth, DEFAULT_QUEUE_SIZE);
    }

    result = xQueueSend(...);
    if (result != pdTRUE) {
        LOG_E(TAG, "Event queue full, event type %u dropped", event.type);
    }

    return result == pdTRUE;
}
```

4. **Consider priority-based event queuing** for critical events:
```cpp
bool EventBus::publish(const Event& event, bool fromISR, bool highPriority = false);
// Use xQueueSendFromISR with higher timeout for critical events
```

## References

- FreeRTOS: Queue usage and sizing guidelines
- ESP-IDF: Event groups and software timers (alternative patterns)
- CDC Badge: Main loop timing (10ms tick)
