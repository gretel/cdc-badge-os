---
title: "[LOW] EventBus process() can starve if queue fills faster than it drains"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "event-handling"
  - "eventbus"
---

## Summary
In `components/cdc_core/src/EventBus.cpp`, the `process()` method uses `xQueueReceive` with zero timeout in a while loop to drain all queued events. If events are continuously published faster than they can be processed, the main loop could spend all its time in `process()` without reaching other critical tasks like `SerialCmd::process()` or `ui_process()`.

**Location:** `components/cdc_core/src/EventBus.cpp:121-139`

```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {  // Zero timeout, non-blocking
        // Dispatch to all matching handlers
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);  // No timeout on handler
                }
            }
        }
    }
}
```

## Impact
- **Starvation risk:** If event publishers are fast, `process()` could monopolize the main task
- **No fairness guarantee:** Other main loop operations (`SerialCmd::process()`, `ui_process()`) may be delayed
- **Handler blocking:** If a handler takes too long, all subsequent events are delayed
- **Potential deadlock:** If a handler publishes events that trigger recursive processing

## Evidence
File: `components/cdc_core/src/EventBus.cpp`
- Lines 121-139: `process()` drains all events with zero timeout
- Line 125: `xQueueReceive(..., 0)` - non-blocking, returns immediately if empty
- Lines 127-138: Nested loop dispatches to all handlers synchronously
- Main loop in `main.cpp:242-256` calls `EventBus::process()` but has no timeout protection

Main loop:
```cpp
while (true) {
    EventBus::instance().process();           // Can block here
    cdc::serial::SerialCmd::process();        // May be delayed
    s_powerManager->update();                 // May be delayed
    cdc::ui::ui_process(nowMs);               // May be delayed
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

## Recommended Fix
Add a processing time limit to prevent starvation:

```cpp
// Add configuration constant
static constexpr uint32_t EVENT_PROCESS_MAX_MS = 5;  // Max 5ms per process() call

/**
 * \brief Drains queued events and dispatches matching handlers.
 * \return Number of events processed.
 */
uint8_t EventBus::process() {
    if (!initialized_ || !queue_) return 0;

    Event event;
    uint8_t processed = 0;
    uint32_t startTime = esp_timer_get_time() / 1000;  // ms

    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        // Check time budget
        uint32_t elapsed = esp_timer_get_time() / 1000 - startTime;
        if (elapsed > EVENT_PROCESS_MAX_MS) {
            LOG_W(TAG, "Event processing time budget exceeded (%lu ms)", elapsed);
            break;  // Stop processing to prevent starvation
        }

        // Dispatch to all matching handlers
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    handlers_[i].handler(event);
                }
            }
        }
        processed++;
    }

    return processed;
}
```

**Alternative: Limit event count per call**
```cpp
// Add configuration constant
static constexpr uint8_t MAX_EVENTS_PER_PROCESS = 10;

uint8_t EventBus::process() {
    if (!initialized_ || !queue_) return 0;

    Event event;
    uint8_t processed = 0;

    while (processed < MAX_EVENTS_PER_PROCESS &&
           xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        // ... dispatch logic ...
        processed++;
    }

    // Log if queue still has events
    if (processed == MAX_EVENTS_PER_PROCESS) {
        LOG_D(TAG, "Event limit reached, %u events remaining", 
              uxQueueMessagesWaiting(queue_));
    }

    return processed;
}
```

**Update main loop to check return value:**
```cpp
// In main.cpp
while (true) {
    uint8_t events = EventBus::instance().process();
    if (events > 0) {
        LOG_D(TAG, "Processed %u events", events);
    }
    
    cdc::serial::SerialCmd::process();
    // ... rest of loop
}
```

## References
- [FreeRTOS Queue Documentation](https://www.freertos.org/Embedded-RTOS-Queues.html)
- [Main Loop Design Patterns](https://www.embedded.com/design/operating-systems/4025678/Designing-a-main-loop-for-embedded-systems)
- ESP-IDF Event Loop: `components/esp_event/include/esp_event.h`
