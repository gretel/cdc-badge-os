---
title: "[MEDIUM] EventBus subscribe() and process() lack mutex protection for handler array"
severity: MEDIUM
domain: concurrency
lens: resource-contention
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `EventBus` class modifies and reads the `handlers_` array in `subscribe()` (line 48-61) and `process()` (line 120-136) without synchronization. While the queue exhaustion issue (002-eventbus-queue-exhaustion.md) addresses capacity, this finding focuses on the **race between handler registration and event dispatch**.

```cpp
// subscribe() - writes handlers_
uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {
            handlers_[i].handler = handler;  // Non-atomic write!
            handlers_[i].mask = mask;
            handlers_[i].active = true;
            return i + 1;
        }
    }
    return 0;
}

// process() - reads handlers_
void EventBus::process() {
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_), &event, 0) == pdTRUE) {
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {  // Non-atomic read!
                handlers_[i].handler(event);
            }
        }
    }
}
```

## Impact
1. **Read-write race**: `process()` may read partially updated `Subscription` struct while `subscribe()` writes.
2. **Lost events**: `process()` could call a `nullptr` handler if it reads between write to `active` and `handler`.
3. **Duplicate dispatch**: Two handlers could get the same slot if `subscribe()` races, causing one to be missed.
4. **Module startup window**: During `ModuleRegistry::startAll()`, modules subscribe while the main loop processes events.

## Evidence
- File: `components/cdc_core/src/EventBus.cpp:48-61` (subscribe writes)
- File: `components/cdc_core/src/EventBus.cpp:120-136` (process reads)
- File: `components/cdc_core/include/cdc_core/EventBus.h:136` (Subscription: 3 fields, not atomic)
- Main loop in `main/main.cpp` calls `EventBus::process()` every 10ms
- Modules may subscribe from any task context during startup

## Recommended Fix
Add mutex protection for both read and write paths (complements queue sizing fix):

```cpp
// In EventBus.h
class EventBus {
private:
    Subscription handlers_[MAX_HANDLERS] = {};
    void* queue_ = nullptr;
    bool initialized_ = false;
    SemaphoreHandle_t mutex_ = nullptr;  // Add mutex
};

// Protect subscribe()
uint8_t EventBus::subscribe(EventHandler handler, uint32_t mask) {
    if (!handler) return 0;
    
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (!handlers_[i].active) {
            handlers_[i].handler = handler;
            handlers_[i].mask = mask;
            handlers_[i].active = true;
            xSemaphoreGive(mutex_);
            return i + 1;
        }
    }
    xSemaphoreGive(mutex_);
    return 0;
}

// Protect process()
void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_), &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);
        
        xSemaphoreTake(mutex_, portMAX_DELAY);
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 || (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);
                }
            }
        }
        xSemaphoreGive(mutex_);
    }
}

// Initialize in init()
bool EventBus::init(size_t queueSize) {
    if (initialized_) return true;
    
    queue_ = xQueueCreate(queueSize, sizeof(Event));
    if (!queue_) return false;
    
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) return false;
    
    initialized_ = true;
    return true;
}
```

## References
- FreeRTOS mutex: https://docs.freertos.org/~/download/Documentation/api/html/group__xSemaphoreCreateMutex.html
- Read-write race patterns: https://www.kernel.org/doc/Documentation/locking/lock-class.txt
