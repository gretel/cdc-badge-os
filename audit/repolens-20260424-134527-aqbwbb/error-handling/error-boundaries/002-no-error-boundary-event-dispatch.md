---
title: "[HIGH] No error boundary around event handler dispatch"
severity: HIGH
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "fault-isolation"
---

## Summary
The `EventBus::process()` function in `components/cdc_core/src/EventBus.cpp:116-134` dispatches events to all subscribed handlers without any error isolation. A single handler throwing an exception or crashing will bring down the event processing loop, stopping all event delivery.

**Evidence:**
- `components/cdc_core/src/EventBus.cpp:120-133`:
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
                    handlers_[i].handler(event);  // No error boundary!
                }
            }
        }
    }
}
```

## Impact
- **System-wide event blackout**: One crashing handler stops all event processing
- **No partial failure**: All subscribers affected by single handler failure
- **Silent failures**: No logging when a handler crashes
- **Debug difficulty**: Hard to identify which handler failed

## Recommended Fix
Add error boundaries around each handler invocation in `EventBus::process()`:

```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);

        for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
            if (handlers_[i].active && handlers_[i].handler) {
                if (handlers_[i].mask == 0 ||
                    (handlers_[i].mask & typeMask)) {
                    try {
                        handlers_[i].handler(event);
                    } catch (const std::exception& e) {
                        LOG_E(TAG, "Event handler exception: %s", e.what());
                    } catch (...) {
                        LOG_E(TAG, "Event handler exception (unknown)");
                    }
                }
            }
        }
    }
}
```

## References
- [Partial Failure Handling](https://opencode.ai/guides/error-handling/partial-failure/)
- [Event-Driven Architecture Best Practices](https://martinfowler.com/articles/patterns-of-distributed-systems/)
