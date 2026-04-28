---
title: "[HIGH] EventBus Publish/Subscribe System Lacks Unit Test Coverage"
severity: HIGH
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `EventBus` class (`components/cdc_core/src/EventBus.cpp`, 142 lines) implements the system-wide publish/subscribe event system but has zero unit tests. Critical untested functions include:

- `init()` (line 25) - Queue initialization
- `subscribe()` (line 48) - Handler registration with event mask
- `unsubscribe()` (line 70) - Handler removal
- `publish()` (line 84) - Event publishing (ISR-safe and task contexts)
- `publish(EventType, uint8_t)` (line 109) - Convenience publishing
- `process()` (line 121) - Event dispatch loop

## Impact
**System Communication Risk:** EventBus is the backbone of inter-module communication:
1. FreeRTOS queue creation can fail - untested error path
2. Handler mask logic (0 = receive all) is untested
3. ISR-safe publishing with `portYIELD_FROM_ISR()` is unproven
4. `process()` dispatch to multiple handlers is untested
5. Handler overflow (MAX_HANDLERS=16) has no tests

## Evidence
File: `components/cdc_core/src/EventBus.cpp`

Line 48-64: `subscribe()` - Handler slot allocation
```cpp
for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
    if (!handlers_[i].active) {
        handlers_[i].handler = handler;
        handlers_[i].mask = mask;
        handlers_[i].active = true;
        return i + 1;  // Return 1-based ID
    }
}
LOG_E(TAG, "No free handler slots");
return 0;  // Untested overflow
```

Line 84-99: `publish()` - ISR vs task context
```cpp
if (fromISR) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    result = xQueueSendFromISR(...);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();  // Untested ISR path
    }
} else {
    result = xQueueSend(..., pdMS_TO_TICKS(10));  // Timeout path untested
}
return result == pdTRUE;
```

Line 121-140: `process()` - Event dispatch
```cpp
while (xQueueReceive(..., 0) == pdTRUE) {  // Non-blocking receive
    uint32_t typeMask = 1u << static_cast<uint8_t>(event.type);
    for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
        if (handlers_[i].active && handlers_[i].handler) {
            if (handlers_[i].mask == 0 || (handlers_[i].mask & typeMask)) {
                handlers_[i].handler(event);  // Called for each match
            }
        }
    }
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "EventBus" {} \;
# Returns nothing - no EventBus tests exist
```

## Recommended Fix
Create `test/test_event_bus/test_event_bus.cpp` with test cases:

1. **Subscription tests:**
   - Test successful subscription returns non-zero ID
   - Test null handler returns 0
   - Test MAX_HANDLERS (16) overflow
   - Test unsubscribe removes handler

2. **Mask tests:**
   - Test mask=0 receives all events
   - Test specific mask receives only matching events
   - Test multiple handlers with different masks

3. **Publish tests:**
   - Test publish returns true on success
   - Test publish with queue full (if possible to simulate)
   - Test convenience `publish(EventType, uint8_t)`

4. **Process tests:**
   - Test process() calls all matching handlers
   - Test process() doesn't call non-matching handlers
   - Test event timestamp is set correctly

Example test:
```cpp
static int handler1_callCount = 0;
static int handler2_callCount = 0;
static Event lastEvent1;

void handler1(const Event& e) {
    handler1_callCount++;
    lastEvent1 = e;
}

void handler2(const Event& e) {
    handler2_callCount++;
}

void test_subscribe_returns_id() {
    auto& bus = EventBus::instance();
    bus.init();
    uint8_t id = bus.subscribe(handler1);
    TEST_ASSERT_GREATER_THAN(0, id);
}

void test_mask_filters_events() {
    auto& bus = EventBus::instance();
    bus.init();
    uint8_t id1 = bus.subscribe(handler1, EventBus::eventMask(EventType::KEY_PRESSED));
    uint8_t id2 = bus.subscribe(handler2, 0);  // All events
    
    bus.publish(EventType::KEY_PRESSED);
    bus.process();
    TEST_ASSERT_EQUAL(1, handler1_callCount);  // Matches mask
    TEST_ASSERT_EQUAL(1, handler2_callCount);  // Receives all
    
    bus.publish(EventType::BLE_CONNECTED);
    bus.process();
    TEST_ASSERT_EQUAL(1, handler1_callCount);  // No match
    TEST_ASSERT_EQUAL(2, handler2_callCount);  // Receives all
}
```

## References
- File: `components/cdc_core/include/cdc_core/EventBus.h` - Full API and EventType enum
- Pattern: Publish/Subscribe with FreeRTOS queue
