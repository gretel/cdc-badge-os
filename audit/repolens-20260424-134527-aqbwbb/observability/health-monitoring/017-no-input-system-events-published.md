---
title: "[MEDIUM] Input and system events defined but never published"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The `EventBus` defines multiple input and system event types that are **never published** in the codebase. These events are meant to track user interactions and system state transitions, but they remain unused.

**Event types defined but not published** (`components/cdc_core/include/cdc_core/EventBus.h:11-37`):
```cpp
enum class EventType : uint8_t {
    // Input events
    KEY_PRESSED,
    KEY_RELEASED,
    KEY_LONG_PRESS,

    // System events
    SYSTEM_UNLOCK,
    SYSTEM_LOCK,
    SYSTEM_SLEEP,
    SYSTEM_WAKE,

    // Timer
    TIMER_TICK,

    // Bluetooth events
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    BLE_PAIRING_REQUEST,
    // ...
};
```

**Current event publishing** (only `MODULE_ERROR` is published):
- `components/cdc_core/src/ModuleRegistry.cpp:689` - Only `MODULE_ERROR` event
- `grep -r "EventType::KEY_" components/` returns 0 matches
- `grep -r "EventType::SYSTEM_" components/` returns 0 matches
- `grep -r "EventType::TIMER_TICK" components/` returns 0 matches

**Key handling exists but no events published** (`components/cdc_os_ui/src/AppUi.cpp`):
- Key processing happens in `ui_process()` but events are not published
- Sleep/wake logic exists in `SleepController` but no events published
- Lock/unlock logic exists but no events published

## Impact

1. **No event-driven UI**: Components cannot subscribe to key events for global shortcuts or animations
2. **Missing observability**: Cannot track user interaction patterns or system state transitions
3. **Inconsistent architecture**: Event types are defined but the pattern is not applied consistently
4. **Debugging difficulty**: Cannot trace user interactions or system state changes via event log
5. **Lost automation opportunities**: Features like "show status on long-press" or "auto-lock on sleep" require polling

## Evidence

**Event definitions** (`components/cdc_core/include/cdc_core/EventBus.h:11-37`):
```cpp
enum class EventType : uint8_t {
    // Input events
    KEY_PRESSED,
    KEY_RELEASED,
    KEY_LONG_PRESS,

    // System events
    SYSTEM_UNLOCK,
    SYSTEM_LOCK,
    SYSTEM_SLEEP,
    SYSTEM_WAKE,

    // Timer
    TIMER_TICK,

    // Bluetooth events
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    BLE_PAIRING_REQUEST,

    // Custom module events
    MODULE_EVENT,
    MODULE_ERROR,
    EVENT_COUNT
};
```

**Key handling code** (`components/cdc_os_ui/src/AppUi.cpp`):
- Key processing happens in `ui_process()` but events are not published
- Sleep/wake logic exists in `SleepController` but no events published

**Event publishing locations** (only MODULE_ERROR):
- `components/cdc_core/src/ModuleRegistry.cpp:689` - Only `MODULE_ERROR` event

**Search results**:
```bash
grep -r "EventType::KEY_" components/  # 0 matches
grep -r "EventType::SYSTEM_" components/  # 0 matches
grep -r "EventType::TIMER_TICK" components/  # 0 matches
grep -r "EventType::BLE_" components/  # 0 matches
```

## Recommended Fix

Add event publishing to key, system, and timer handling code:

**Step 1: Publish key events** (`components/cdc_os_ui/src/AppUi.cpp`):
```cpp
#include "cdc_core/EventBus.h"

static void onKey(uint8_t key, bool pressed) {
    // Publish key event
    auto evt = pressed ? cdc::core::EventType::KEY_PRESSED : cdc::core::EventType::KEY_RELEASED;
    cdc::core::Event e = {};
    e.type = evt;
    e.data.key = key;
    cdc::core::EventBus::instance().publish(e);
    
    // Existing key processing...
}
```

**Step 2: Publish system state events** (`components/cdc_hal/src/SleepController.cpp`):
```cpp
void SleepController::enterSleep() {
    // Publish sleep event
    cdc::core::Event e = {};
    e.type = cdc::core::EventType::SYSTEM_SLEEP;
    cdc::core::EventBus::instance().publish(e);
    
    // Existing sleep logic...
}

void SleepController::wake() {
    // Publish wake event
    cdc::core::Event e = {};
    e.type = cdc::core::EventType::SYSTEM_WAKE;
    cdc::core::EventBus::instance().publish(e);
    
    // Existing wake logic...
}
```

**Step 3: Publish lock/unlock events** (`components/cdc_os_ui/src/LockScreenView.cpp`):
```cpp
void LockScreenView::unlock() {
    // Publish unlock event
    cdc::core::Event e = {};
    e.type = cdc::core::EventType::SYSTEM_UNLOCK;
    cdc::core::EventBus::instance().publish(e);
    
    // Existing unlock logic...
}
```

**Step 4: Publish timer tick events** (main loop):
```cpp
// In main.cpp main loop
while (true) {
    // ... existing processing ...
    
    // Publish timer tick (e.g., every second)
    static uint32_t lastTickMs = 0;
    uint32_t nowMs = esp_timer_get_time() / 1000;
    if (nowMs - lastTickMs >= 1000) {
        lastTickMs = nowMs;
        cdc::core::Event e = {};
        e.type = cdc::core::EventType::TIMER_TICK;
        cdc::core::EventBus::instance().publish(e);
    }
    
    // ...
}
```

**Step 5: Subscribe to events** (example subscriber):
```cpp
// AppUi.cpp or similar
static void onKeyEvent(const cdc::core::Event& evt) {
    if (evt.type == cdc::core::EventType::KEY_PRESSED) {
        char key = evt.data.key;
        LOG_I("KEY", "Key pressed: %c", key);
    }
}

// Subscribe in init
cdc::core::EventBus::instance().subscribe(onKeyEvent, 
    cdc::core::EventBus::eventMask(cdc::core::EventType::KEY_PRESSED));
```

**Expected event flow**:
```
KEY_PRESSED (A) -> KEY_LONG_PRESS (A) -> SYSTEM_LOCK -> SYSTEM_SLEEP -> 
SYSTEM_WAKE -> SYSTEM_UNLOCK -> KEY_RELEASED (A)
```

## References

- Event-driven architecture patterns: https://www.enterpriseintegrationpatterns.com/patterns/messaging/
- FreeRTOS software timers: https://www.freertos.org/FreeRTOS-quick-start.html
- ESP32 input handling: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html
