---
title: "[MEDIUM] Missing Event Schema Definition - MODULE_EVENT Uses Generic Union"
severity: MEDIUM
domain: architecture/api-contract
lens: event-schema-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `EventBus` uses a generic union for event data (`char key`, `uint8_t value`, `void* ptr`) with no schema definition for `MODULE_EVENT` type. Consumers must implicitly know how to interpret the data based on the event type, leading to fragile assumptions and potential type mismatches.

## Impact
- **Type safety violations**: No compile-time checking of event data structure.
- **Fragile consumers**: Modules must know the exact data format for each event type.
- **No self-documentation**: Event structure is not visible in the interface definition.
- **Extension difficulty**: Adding new event fields requires coordinated changes across all consumers.

## Evidence
**Event definition - `components/cdc_core/include/cdc_core/EventBus.h:47-58`:**
```cpp
struct Event {
    EventType type;
    uint32_t timestamp;  // millis since boot
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
    } data;
};
```

**Event types - `components/cdc_core/include/cdc_core/EventBus.h:11-44`:**
```cpp
enum class EventType : uint8_t {
    // Input events
    KEY_PRESSED,
    KEY_RELEASED,
    KEY_LONG_PRESS,

    // Power events
    POWER_USB_CONNECTED,
    POWER_USB_DISCONNECTED,
    POWER_CHARGING,
    POWER_BATTERY_LOW,
    POWER_BATTERY_CRITICAL,

    // System events
    SYSTEM_UNLOCK,
    SYSTEM_LOCK,
    SYSTEM_SLEEP,
    SYSTEM_WAKE,

    // Bluetooth events
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    BLE_PAIRING_REQUEST,

    // Timer
    TIMER_TICK,

    // Custom module events (use data.value for sub-type)
    MODULE_EVENT,

    // Module error (data.ptr = module name, data.value = index)
    MODULE_ERROR,

    EVENT_COUNT
};
```

**Event publishing - `components/cdc_core/include/cdc_core/EventBus.h:108-112`:**
```cpp
bool publish(const Event& event, bool fromISR = false);
bool publish(EventType type, uint8_t value = 0);  // Convenience method
```

**Comment in EventBus.h implies implicit contract:**
```cpp
// Custom module events (use data.value for sub-type)
MODULE_EVENT,

// Module error (data.ptr = module name, data.value = index)
MODULE_ERROR,
```

**Event consumption pattern (implicit assumptions):**
```cpp
// Consumer must know:
// - For MODULE_ERROR: data.ptr is module name string, data.value is index
// - For MODULE_EVENT: data.value is sub-type (what sub-types exist?)
void onEvent(const Event& event) {
    switch (event.type) {
        case MODULE_ERROR:
            const char* moduleName = (const char*)event.data.ptr;
            uint8_t index = event.data.value;
            // What if ptr is not null-terminated? No length field!
            break;
        case MODULE_EVENT:
            uint8_t subType = event.data.value;
            // What are valid subTypes? No enum defined!
            break;
    }
}
```

## Recommended Fix
**Define structured event data with explicit schemas:**

1. **Create event data structures:**
```cpp
struct ModuleEventData {
    uint8_t moduleId;
    uint8_t subType;
    uint16_t flags;
    void* context;  // Optional context pointer
};

struct ModuleErrorData {
    const char* moduleName;
    uint8_t errorIndex;
    uint8_t severity;  // 0=info, 1=warning, 2=error, 3=critical
    const char* message;  // Optional error message
};
```

2. **Update Event structure to include typed unions.**

3. **Add typed publish methods:**
```cpp
bool publishModuleEvent(uint8_t moduleId, uint8_t subType, void* context = nullptr);
bool publishModuleError(const char* moduleName, uint8_t errorIndex, 
                        uint8_t severity = 1, const char* message = nullptr);
```

4. **Define module event sub-types in each module.**

## References
- EventBus: `components/cdc_core/include/cdc_core/EventBus.h`
- Event types: `components/cdc_core/include/cdc_core/EventBus.h:11-44`
