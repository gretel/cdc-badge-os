---
title: "[LOW] Event types are hardcoded enum requiring core modification for new events"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
Event types are defined as a fixed enum in `components/cdc_core/include/cdc_core/EventBus.h:8-38`. Adding a new event type requires modifying the core `EventBus.h` file, which violates the Open/Closed Principle.

**Evidence:**
- `components/cdc_core/include/cdc_core/EventBus.h:8-38` - Hardcoded `EventType` enum with 15+ event types

## Impact
- **Extension Barrier**: New event types (e.g., `MODULE_INIT_COMPLETE`, `BACKUP_CREATED`) require editing core code
- **Coupling**: All event publishers/subscribers depend on the core enum
- **Scalability**: As modules grow, more event types will be needed

## Evidence
File: `components/cdc_core/include/cdc_core/EventBus.h:8-38`
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

Note the `MODULE_EVENT` hack: "Custom module events (use data.value for sub-type)" - this is a workaround for the inability to register custom event types.

## Recommended Fix
Implement dynamic event type registration:

1. **Change to string-based or ID-based event types**:
   ```cpp
   struct EventType {
       uint8_t id;           // Auto-assigned unique ID
       const char* name;     // Human-readable name
       size_t dataSize;      // Size of associated data
   };
   ```

2. **Add registration API**:
   ```cpp
   class EventRegistry {
   public:
       static EventRegistry& instance();
       uint8_t registerType(const char* name, size_t dataSize = 0);
       EventType* getTypeById(uint8_t id);
       EventType* getTypeByName(const char* name);
   };
   ```

3. **Pre-define standard events**:
   ```cpp
   void registerStandardEventTypes() {
       EventRegistry::instance().registerType("KEY_PRESSED", sizeof(KeyEventData));
       EventRegistry::instance().registerType("SYSTEM_UNLOCK", 0);
       // ...
   }
   ```

4. **Allow modules to register custom events**:
   ```cpp
   // In a module that wants custom events
   struct BackupEventData {
       uint16_t slotCount;
       uint32_t timestamp;
   };
   
   static uint8_t s_backupEventId = EventRegistry::instance()
       .registerType("BACKUP_CREATED", sizeof(BackupEventData));
   ```

5. **Update EventBus to use dynamic types**:
   ```cpp
   class EventBus {
   public:
       bool publish(uint8_t eventType, const void* data = nullptr);
       uint8_t subscribe(EventHandler handler, uint8_t eventType);  // Single type
       uint8_t subscribeAll(EventHandler handler);  // All types
   };
   ```

This allows modules to define their own event types without editing core code.

## References
- Plugin Architecture: Dynamic extension points
- Event-Driven Architecture: Decoupled communication
- Registry Pattern: Centralized management of extensible items
