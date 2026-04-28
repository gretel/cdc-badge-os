---
title: "[HIGH] EventBus integration lacks end-to-end tests for module communication"
severity: HIGH
domain: core
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_core"
  - "area:event-bus"
---

## Summary
The `EventBus` component (`components/cdc_core/EventBus.h/cpp`) provides a publish/subscribe mechanism for inter-module communication, but there are **no integration tests** verifying that events flow correctly between publishers and subscribers.

Current tests only cover basic symbol linking (e.g., `test_vcard_module_link`), not actual event flow.

## Impact
- **Silent integration failures**: Events may be published but not received due to subscription timing issues
- **Race conditions**: Module initialization order could cause events to be lost if subscribed too late
- **No regression coverage**: Changes to event types or handler logic have no verification

## Evidence

**EventBus API** (`components/cdc_core/include/cdc_core/EventBus.h:72-119`):
```cpp
class EventBus {
    uint8_t subscribe(EventHandler handler, uint32_t mask = 0);
    bool publish(const Event& event, bool fromISR = false);
    void process();  // Must be called from main loop
};
```

**Event Types** (`components/cdc_core/include/cdc_core/EventBus.h:11-44`):
```cpp
enum class EventType : uint8_t {
    KEY_PRESSED, KEY_RELEASED, KEY_LONG_PRESS,
    POWER_USB_CONNECTED, POWER_USB_DISCONNECTED,
    SYSTEM_UNLOCK, SYSTEM_LOCK,
    BLE_CONNECTED, BLE_DISCONNECTED,
    MODULE_EVENT, MODULE_ERROR,
    // ...
};
```

**Usage in main loop** (`main/main.cpp:239-240`):
```cpp
while (true) {
    EventBus::instance().process();
    // ...
}
```

**Current test coverage** (`test/test_vcard_module_link/test_vcard_module_link.cpp`):
```cpp
void test_vcard_module_link() {
    mod_vcard_register();  // Just calls registration, no event testing
}
```

## Recommended Fix

Create integration test `test_event_bus_integration/` that verifies:

1. **Basic publish/subscribe**: Handler receives event after publish
2. **Event masking**: Subscribers with masks only receive matching events
3. **Multiple subscribers**: All matching handlers are called
4. **ISR-safe publishing**: Events from ISR context are queued correctly
5. **Event ordering**: Events are processed in FIFO order
6. **Module lifecycle events**: Verify `SYSTEM_UNLOCK`, `SYSTEM_LOCK` events reach modules

**Test structure** (example):
```cpp
// test/test_event_bus_integration/test_event_bus.cpp
#include "cdc_core/EventBus.h"

static bool s_handlerCalled = false;
static Event s_lastEvent;

static void onEvent(const Event& event) {
    s_handlerCalled = true;
    s_lastEvent = event;
}

void test_event_bus_publish_subscribe() {
    EventBus& bus = EventBus::instance();
    bus.init();
    
    uint8_t id = bus.subscribe(onEvent);
    ASSERT_GT(id, 0);
    
    bus.publish(EventType::SYSTEM_UNLOCK);
    bus.process();
    
    ASSERT_TRUE(s_handlerCalled);
    ASSERT_EQ(s_lastEvent.type, EventType::SYSTEM_UNLOCK);
}
```

## References
- [EventBus header](components/cdc_core/include/cdc_core/EventBus.h)
- [EventBus implementation](components/cdc_core/src/EventBus.cpp)
- [Main loop integration](main/main.cpp:239)
