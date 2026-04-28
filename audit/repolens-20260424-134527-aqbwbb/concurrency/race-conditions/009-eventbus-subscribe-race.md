---
title: "[MEDIUM] EventBus Subscription Array Race Condition"
severity: MEDIUM
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `EventBus` component (`components/cdc_core/src/EventBus.cpp`) has a race condition in the `subscribe()` method where the handler array is accessed without synchronization. If two tasks subscribe simultaneously, they may overwrite each other or get duplicate handler IDs.

**Location**: `components/cdc_core/include/cdc_core/EventBus.h` and `components/cdc_core/src/EventBus.cpp` (lines 47-60)

## Impact

**Functional Bug**: Concurrent subscription calls can result in:
1. Two handlers getting the same ID (collision)
2. One handler overwriting another
3. Lost subscriptions

**Debugging Difficulty**: The issue is intermittent and depends on task scheduling, making it hard to reproduce.

## Evidence

### Unprotected Subscription

`components/cdc_core/src/EventBus.cpp` line 47-60:
```cpp
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
```

### Concurrent Access Pattern

The `subscribe()` method performs a read-modify-write sequence:
1. **Read**: Check `handlers_[i].active`
2. **Modify**: Set `handlers_[i].handler`, `handlers_[i].mask`, `handlers_[i].active`

Between step 1 and 2, another task could:
- Find the same slot as "free"
- Write its own handler to the same slot

### Race Scenario

```
Task A: subscribe(handlerA, maskA)
        -> checks handlers_[0].active (false)
Task B: subscribe(handlerB, maskB)
        -> checks handlers_[0].active (false, A hasn't written yet)
        -> writes handlerB to handlers_[0]
        -> returns ID 1
Task A: -> writes handlerA to handlers_[0]
        -> returns ID 1  (collision!)
```

Now both tasks have ID 1, and only handlerA survives in the array.

### Unprotected Unsubscribe

`components/cdc_core/src/EventBus.cpp` line 69-75:
```cpp
void EventBus::unsubscribe(uint8_t id) {
    if (id == 0 || id > MAX_HANDLERS) return;

    handlers_[id - 1].active = false;
    handlers_[id - 1].handler = nullptr;
    LOG_D(TAG, "Handler %u unsubscribed", id);
}
```

Same issue - no synchronization.

### Event Dispatch Also Unprotected

`components/cdc_core/src/EventBus.cpp` line 118-135:
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
                // Check mask (0 = receive all)
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);  // Could be called while unsubscribing
                }
            }
        }
    }
}
```

During dispatch, a handler could be unsubscribed, leading to:
- Calling a handler that was just removed
- Accessing `handlers_[i].handler` while another task sets it to `nullptr`

## Recommended Fix

### Add Critical Section to Subscribe/Unsubscribe

`components/cdc_core/include/cdc_core/EventBus.h`:
```cpp
class EventBus {
private:
    // ... existing members ...
    static portMUX_TYPE s_mux;  // Add this
};
```

`components/cdc_core/src/EventBus.cpp`:
```cpp
static const char* TAG = "EventBus";
static portMUX_TYPE EventBus::s_mux = portMUX_INITIALIZER_UNLOCKED;

uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    if (!handler) return 0;

    portENTER_CRITICAL(&s_mux);
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {
            handlers_[i].handler = handler;
            handlers_[i].mask = mask;
            handlers_[i].active = true;
            portEXIT_CRITICAL(&s_mux);
            LOG_D(TAG, "Handler %u subscribed (mask: 0x%08lx)", i + 1, mask);
            return i + 1;
        }
    }
    portEXIT_CRITICAL(&s_mux);

    LOG_E(TAG, "No free handler slots");
    return 0;
}

void EventBus::unsubscribe(uint8_t id) {
    if (id == 0 || id > MAX_HANDLERS) return;

    portENTER_CRITICAL(&s_mux);
    handlers_[id - 1].active = false;
    handlers_[id - 1].handler = nullptr;
    portEXIT_CRITICAL(&s_mux);
    LOG_D(TAG, "Handler %u unsubscribed", id);
}

void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        // Copy handlers to stack to minimize critical section
        struct {
            EventHandler handler;
            uint32_t mask;
            bool active;
        } snapshot[MAX_HANDLERS];

        portENTER_CRITICAL(&s_mux);
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            snapshot[i] = handlers_[i];
        }
        portEXIT_CRITICAL(&s_mux);

        // Dispatch using snapshot
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (snapshot[i].active && snapshot[i].handler) {
                if (snapshot[i].mask == 0 ||
                    (snapshot[i].mask & typeMask)) {
                    snapshot[i].handler(event);
                }
            }
        }
    }
}
```

### Alternative: Use Semaphore for Longer Critical Sections

For more complex handlers, a mutex might be better:
```cpp
static SemaphoreHandle_t s_mutex = nullptr;

// In init():
s_mutex = xSemaphoreCreateMutex();

// In subscribe/unsubscribe/process:
xSemaphoreTake(s_mutex, portMAX_DELAY);
// ... access handlers_ ...
xSemaphoreGive(s_mutex);
```

## References

- ESP-IDF Critical Sections: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/critical_section.html
- FreeRTOS portMUX for ISR-safe locks: https://www.freertos.org/a00111.html
- Event-driven concurrency patterns: https://www.oreilly.com/library/view/programming-esp32/9781492042819/ch04.html
