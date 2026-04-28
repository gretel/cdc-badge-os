---
title: "[MEDIUM] Missing event flow and inter-module communication documentation"
severity: MEDIUM
domain: architecture
lens: architecture-docs
labels:
  - "audit:documentation/architecture-docs"
---

## Summary
The system uses an `EventBus` for system-wide communication and `ServiceRegistry` for typed service discovery, but there is no documentation describing:

1. **Event types** - Complete list of `EventType` values and when each is published
2. **Event consumers** - Which components subscribe to which events
3. **Service contracts** - What each typed service interface provides and how to use it
4. **Message flow** - How events propagate through the system

**Evidence:**
- `components/cdc_core/include/cdc_core/EventBus.h:11-42` - Defines 15+ event types but no usage documentation
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:11-17` - Defines `ServiceType` enum with only 3 types (KEYBOARD, CLIPBOARD, NOTIFICATION)
- `components/cdc_core/include/cdc_core/IKeyboardProvider.h` - Interface exists but no usage examples
- `docs/MODULE_DEVELOPMENT.md` - Mentions events but doesn't document the full system
- No documentation of which modules publish/consume which events

## Impact
- Developers cannot easily discover how modules communicate
- Hard to add new event types or services without breaking existing code
- New module authors must read source code to understand communication patterns
- Risk of event naming collisions or duplicate event types
- Unclear when to use EventBus vs ServiceRegistry vs direct module calls

## Evidence
**EventBus definition (`EventBus.h:11-42`):**
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

**ServiceRegistry typed services (`ServiceRegistry.h:11-17`):**
```cpp
enum class ServiceType {
    KEYBOARD,      // IKeyboardProvider - keyboard input (BLE HID, USB HID, etc.)
    CLIPBOARD,     // Future: clipboard access
    NOTIFICATION,  // Future: push notifications
};
```

**Current documentation gap:**
- `docs/MODULE_DEVELOPMENT.md:63-68` - Mentions IKeyboardProvider pattern but no full explanation
- No list of which modules provide/consume which services
- No documentation of event subscription patterns in modules

## Recommended Fix
Add a new document `docs/event-flows.md` with:

**1. Event Types Reference**
| Event | Published By | Consumed By | Data Fields |
|-------|--------------|-------------|-------------|
| KEY_PRESSED | IKeypad implementation | AppUi, ModuleRegistry | data.key = pressed key |
| SYSTEM_UNLOCK | PinManager | All modules (via onUnlock()) | - |
| POWER_USB_CONNECTED | USB controller | mod_fido2, mod_gpg | - |

**2. Event Flow Examples**
```
User presses Y key
  → IKeypad::scanKeys() detects key
  → EventBus.publish(KEY_PRESSED, 'Y')
  → AppUi handles event → shows PIN entry
  → PinManager validates PIN
  → EventBus.publish(SYSTEM_UNLOCK)
  → All modules receive onUnlock() callback
  → Main menu displayed
```

**3. Typed Services Reference**
| Service | Provided By | Used By | Interface Methods |
|---------|-------------|---------|-------------------|
| KEYBOARD | mod_hid (BLE HID) | mod_totp (auto-type) | `typeString()`, `isConnected()` |

**4. When to Use Each Pattern**
- **EventBus**: One-to-many, decoupled communication (system events)
- **ServiceRegistry**: Provider-consumer pattern (keyboard, clipboard)
- **Direct calls**: Tight coupling acceptable (view → model)

**5. Module Communication Patterns**
- How `mod_totp` uses keyboard for auto-type
- How `mod_vcard` broadcasts via BLE
- How modules handle USB connect/disconnect

## References
- EventBus API: `components/cdc_core/include/cdc_core/EventBus.h`
- ServiceRegistry API: `components/cdc_core/include/cdc_core/ServiceRegistry.h`
- IKeyboardProvider: `components/cdc_core/include/cdc_core/IKeyboardProvider.h`
- Module event callbacks: `components/cdc_core/include/cdc_core/IModule.h`
