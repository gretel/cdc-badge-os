---
title: "[MEDIUM] EventType Enum Requires Core Modification for New Events"
severity: MEDIUM
domain: extensibility
lens: event-system
labels:
  - "audit:architecture/extensibility"
---

## Summary
The `EventType` enum in `components/cdc_core/include/cdc_core/EventBus.h:8-44` is a fixed list that requires modifying the enum to add new event types. Each new event type needs:
1. Adding a new enum value
2. Potentially updating `subscribe()` mask logic
3. Adding handler cases wherever events are processed

**Evidence:**
- `components/cdc_core/include/cdc_core/EventBus.h:8-44`:
  ```cpp
  enum class EventType : uint8_t {
      KEY_PRESSED,
      KEY_RELEASED,
      KEY_LONG_PRESS,
      POWER_USB_CONNECTED,
      POWER_USB_DISCONNECTED,
      // ... more fixed types
      MODULE_EVENT,     // Generic with sub-type in data.value
      MODULE_ERROR,
      EVENT_COUNT
  };
  ```

- `components/cdc_os_ui/src/AppUi.cpp:688`: Event subscription uses hardcoded type:
  ```cpp
  core::EventBus::instance().subscribe(onModuleErrorEvent, static_cast<uint32_t>(core::EventType::MODULE_ERROR));
  ```

## Impact
**Limited Extensibility:** Modules cannot define their own event types without modifying the core `EventType` enum. This requires:
1. Core code modification for every new event type
2. Risk of enum value collisions
3. Need to rebuild core component for module-specific events

The `MODULE_EVENT` generic type exists but requires manual sub-type management in `data.value`.

## Evidence
Files affected:
- `components/cdc_core/include/cdc_core/EventBus.h:8-44` (enum definition)
- `components/cdc_os_ui/src/AppUi.cpp:688` (event subscription)
- `components/cdc_core/src/EventBus.cpp` (event processing)

Event mask creation in `EventBus.h:117-120`:
```cpp
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```
This assumes `EventType` values fit in a 32-bit mask, limiting to 32 types.

## Recommended Fix
Implement a registry-based event system:

1. **Add EventTypeId and EventRegistry:**
   ```cpp
   using EventTypeId = uint16_t;
   
   class EventRegistry {
   public:
       EventTypeId registerEventType(const char* name, uint8_t moduleCount = 1);
       const char* getEventName(EventTypeId id);
   };
   ```

2. **Modules register their own events:**
   ```cpp
   // In module init
   static EventTypeId s_myEvent = EventRegistry::instance().registerEventType("mod_totp_code_refresh");
   ```

3. **Update EventBus to use dynamic types:**
   ```cpp
   struct Event {
       EventTypeId typeId;
       uint32_t timestamp;
       union { char key; uint8_t value; void* ptr; } data;
   };
   ```

This allows modules to define events without modifying core code.

## References
- Event-driven architecture patterns
- Observer Pattern
- Type-safe event systems
