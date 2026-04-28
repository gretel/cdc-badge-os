---
title: "[LOW] Missing logging for event dispatch in EventBus"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The `EventBus` component (`components/cdc_core/src/EventBus.cpp`) lacks logging for event dispatch, making it difficult to trace event flow through the system during debugging.

**Missing log entries:**

1. **Event publishing** (lines 83-96): No log when events are published:
```cpp
bool EventBus::publish(const Event& event, bool fromISR) {
    if (!initialized_ || !queue_) return false;
    ...
    result = xQueueSend(static_cast<QueueHandle_t>(queue_), &event, pdMS_TO_TICKS(10));
    return result == pdTRUE;  // No LOG_D for publish event
}
```

2. **Event processing** (lines 118-134): No logging of event dispatch:
```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;
    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_), &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 || (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);  // No LOG_D for dispatch
                }
            }
        }
    }
}
```

3. **Queue full/dropped events**: No tracking when queue sends fail

## Impact
- **Debug difficulty**: Hard to trace event flow without log entries
- **System understanding**: Event-driven architecture is harder to understand without visibility
- **Performance tuning**: Can't easily identify if events are being dropped due to queue full conditions

## Evidence
The EventBus is a core component that drives inter-module communication:
```cpp
// main.cpp line 243: Main loop calls EventBus::process()
while (true) {
    EventBus::instance().process();
    ...
}
```

Without logging, it's impossible to see:
- What events are being published
- Which handlers are receiving events
- If events are being dropped

## Recommended Fix
Add debug-level logging to EventBus:

1. **In `publish()` function (around line 95)**:
```cpp
bool EventBus::publish(const Event& event, bool fromISR) {
    if (!initialized_ || !queue_) return false;
    
    BaseType_t result;
    if (fromISR) {
        ...
    } else {
        result = xQueueSend(static_cast<QueueHandle_t>(queue_), &event, pdMS_TO_TICKS(10));
    }
    
    if (result == pdTRUE) {
        LOG_D(TAG, "Published event type=%u", (unsigned)event.type);
    } else {
        LOG_W(TAG, "Event queue full, event type=%u dropped", (unsigned)event.type);
    }
    return result == pdTRUE;
}
```

2. **In `process()` function (around line 120)**:
```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;
    
    Event event;
    int processed = 0;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_), &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);
        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 || (handlers_[i].mask & typeMask)) {
                    LOG_D(TAG, "Dispatching event type=%u to handler %u", (unsigned)event.type, i + 1);
                    handlers_[i].handler(event);
                }
            }
        }
        processed++;
    }
    if (processed > 0) {
        LOG_D(TAG, "Processed %d events", processed);
    }
}
```

Note: Use `LOG_D` (DEBUG level) to avoid cluttering logs during normal operation.

## References
- `components/cdc_core/src/EventBus.cpp` - Event bus implementation
- `components/cdc_core/include/cdc_core/EventBus.h` - Event bus interface
