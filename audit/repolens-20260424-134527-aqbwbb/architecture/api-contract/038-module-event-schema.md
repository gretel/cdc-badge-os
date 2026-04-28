---
title: "[MEDIUM] Implicit Module Event Schema with No Documentation"
severity: MEDIUM
domain: API Contract Integrity
lens: event-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
Modules publish and subscribe to events via `EventBus`, but the event data structure (`Event.data`) is a union with no schema. Modules implicitly assume what data goes in `data.ptr`, `data.value`, etc., without any shared definition or documentation.

**Location**: `components/cdc_core/include/cdc_core/EventBus.h:43-58` (Event definition)

## Impact
1. **Data interpretation bugs**: Modules may misinterpret event payload structure
2. **Breaking changes**: Adding new event types requires manual coordination
3. **Debugging difficulty**: Hard to trace event flow without schema

## Evidence

In `components/cdc_core/include/cdc_core/EventBus.h:43-58`:
```cpp
/**
 * Event types for system-wide communication
 */
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

/**
 * Event data structure
 */
struct Event {
    EventType type;
    uint32_t timestamp;  // millis_since_boot
    union {
        char key;        // For KEY_* events
        uint8_t value;   // Generic value
        void* ptr;       // Pointer to extended data
    } data;
};
```

**The contract for `MODULE_ERROR` is documented**:
```cpp
// Module error (data.ptr = module name, data.value = index)
MODULE_ERROR,
```

**But `MODULE_EVENT` is vague**:
```cpp
// Custom module events (use data.value for sub-type)
MODULE_EVENT,
```

No specification for:
- What sub-types exist?
- What other fields in `data` mean?
- How to extend with new event types?

In `components/cdc_os_ui/src/AppUi.cpp:47-51`:
```cpp
static void onModuleErrorEvent(const core::Event& event) {
    const char* moduleName = static_cast<const char*>(event.data.ptr);
    uint8_t moduleIndex = event.data.value;
    // ... handle error
}
```

This assumes `MODULE_ERROR` uses `data.ptr` for module name and `data.value` for index. What if another module uses different fields?

In `components/cdc_core/src/ModuleRegistry.cpp:215-220`:
```cpp
void ModuleRegistry::dispatchUnlock() {
    core::Event evt;
    evt.type = core::EventType::SYSTEM_UNLOCK;
    evt.timestamp = core::UsbManager::instance().getTickCount();
    core::EventBus::instance().publish(evt);
}
```

No documentation on what `SYSTEM_UNLOCK` payload should contain (currently empty, but what if we need to add user ID?).

## Recommended Fix

1. **Define event payload schemas** for each event type:
   ```cpp
   /**
    * \brief Event payload structure with type-specific data.
    */
   union EventPayload {
       // KEY_* events
       struct {
           char key;           // Key code
           bool shifted;       // Shift key state
       } keyEvent;

       // POWER_* events
       struct {
           uint8_t batteryLevel;  // 0-100%
           bool isCharging;       // Charging state
       } powerEvent;

       // MODULE_ERROR event
       struct {
           const char* moduleName;  // Module name
           uint8_t moduleIndex;     // Module index
           const char* errorCode;   // Error code string
       } moduleError;

       // MODULE_EVENT (generic)
       struct {
           uint8_t moduleId;        // Module ID
           uint8_t eventType;       // Module-specific event type
           void* payload;           // Optional extended data
           uint16_t payloadSize;    // Payload size in bytes
       } moduleEvent;

       // SYSTEM_* events
       struct {
           uint32_t timestamp;      // Event timestamp
           uint8_t userId;          // User ID (for lock/unlock)
       } systemEvent;
   };

   struct Event {
       EventType type;
       uint32_t timestamp;
       EventPayload data;  // Type-specific payload
   };
   ```

2. **Document each event type** with payload schema:
   ```cpp
   enum class EventType : uint8_t {
       // ...
       /**
        * \brief MODULE_ERROR event.
        * Payload: moduleError struct
        * - moduleName: Name of module with error
        * - moduleIndex: Index in module registry
        * - errorCode: Error code string (optional)
        */
       MODULE_ERROR,
   };
   ```

3. **Add event builder helpers**:
   ```cpp
   /**
    * \brief Create MODULE_ERROR event.
    * \param moduleName Module name.
    * \param moduleIndex Module index.
    * \param errorCode Error code string (optional).
    * \return Event ready for publishing.
    */
   static inline Event createModuleErrorEvent(const char* moduleName,
                                              uint8_t moduleIndex,
                                              const char* errorCode = nullptr) {
       Event evt;
       evt.type = EventType::MODULE_ERROR;
       evt.data.moduleError.moduleName = moduleName;
       evt.data.moduleError.moduleIndex = moduleIndex;
       evt.data.moduleError.errorCode = errorCode;
       return evt;
   }
   ```

4. **Define module event sub-types** in each module:
   ```cpp
   // components/mod_fido2/include/mod_fido2/Fido2Events.h
   namespace mod_fido2 {
       enum class FidoEvent : uint8_t {
           CREDENTIAL_CREATED = 0,
           CREDENTIAL_DELETED = 1,
           AUTHENTICATED = 2,
       };
   }
   ```

## References
- `components/cdc_core/include/cdc_core/EventBus.h:43-58` - Event definition
- `components/cdc_core/src/ModuleRegistry.cpp:215-220` - Event dispatch
- `components/cdc_os_ui/src/AppUi.cpp:47-51` - Event consumption
