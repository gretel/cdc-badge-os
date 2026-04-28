---
title: "[LOW] Event Bus Processing Only in Main Loop, Not After Registration"
severity: LOW
domain: startup-perf
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The EventBus is initialized early (line 73 in `main/main.cpp`) and modules can publish events during their initialization. However, `EventBus::process()` is only called in the main loop (line 241), meaning events published during module initialization are queued but not processed until the first iteration of the main loop. This can cause delays in event-driven startup logic.

**Evidence:**
```cpp
// main/main.cpp:69-74
if (!EventBus::instance().init()) {
    return;
}

// ... module initialization ...
modules_register_all();
ModuleRegistry::instance().runAllInitializers();  // Modules may publish events here

// ... main loop ...
while (true) {
    EventBus::instance().process();  // First processing at line 241
    // ...
}
```

## Impact
- **Event Latency**: Events published during boot wait ~10-20ms (first loop iteration) before being processed
- **Dependency Ordering**: Modules expecting event-driven initialization may have timing issues
- **UI Updates**: Toast notifications or status updates during boot may appear delayed

## Evidence
File: `main/main.cpp` lines 69-74, 241
- EventBus initialized at line 73
- First `process()` call at line 241 (after ~150 lines of initialization)
- Modules publish events during init (e.g., `ModuleRegistry::reportModuleError()` at line 690)

File: `components/cdc_core/src/EventBus.cpp` lines 118-137
```cpp
void EventBus::process() {
    if (!initialized_ || !queue_) return;

    Event event;
    while (xQueueReceive(static_cast<QueueHandle_t>(queue_),
                         &event, 0) == pdTRUE) {
        // Dispatch to all matching handlers
        // ...
    }
}
```

## Recommended Fix
Process events periodically during boot initialization:

```cpp
// Add helper after each major initialization block:
static void processEvents() {
    EventBus::instance().process();
    vTaskDelay(pdMS_TO_TICKS(2));  // Small delay to batch events
}

// Call after key initialization points:
// After USB init
usb_cdc_init();
processEvents();

// After I2C init
s_i2cBus = cdc::hal::getI2cBus0();
processEvents();

// After module registration
modules_register_all();
ModuleRegistry::instance().runAllInitializers();
processEvents();  // Process module events before continuing
```

Alternatively, process events more frequently in the main loop:
```cpp
// In main loop, process more often:
while (true) {
    EventBus::instance().process();
    // ... other processing ...
    EventBus::instance().process();  // Second pass
    vTaskDelay(pdMS_TO_TICKS(5));  // Shorter delay
}
```

This reduces event latency during boot from ~20ms to ~2ms.

## References
- FreeRTOS queue operations are fast (<1ms)
- Processing events more frequently improves perceived responsiveness
- Similar pattern used in other embedded UI frameworks
