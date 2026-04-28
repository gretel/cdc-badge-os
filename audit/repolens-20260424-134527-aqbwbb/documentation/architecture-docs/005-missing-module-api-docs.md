---
title: "[MEDIUM] Missing module API contract documentation"
severity: MEDIUM
domain: architecture
lens: architecture-docs
labels:
  - "audit:documentation/architecture-docs"
---

## Summary
Module interfaces are defined in code but not documented as API contracts. There is no comprehensive documentation of:

1. **IModule interface** - All methods, parameters, and expected behavior
2. **Module lifecycle** - init() → start() → stop() sequence and when each is called
3. **Optional callbacks** - onUnlock(), onLock(), onUsbConnect(), etc.
4. **Menu registration** - How modules expose menu items
5. **Slot allocation** - How modules request and receive TROPIC01 slots

**Evidence:**
- `components/cdc_core/include/cdc_core/IModule.h` - Interface defined but only minimal inline comments
- `docs/MODULE_DEVELOPMENT.md` - Tutorial-style guide but no API reference
- No generated API documentation from Doxygen for module interfaces
- No contract specification for what modules must implement

## Impact
- Module developers must read source code to understand interface
- Unclear which methods are required vs optional
- No documentation of lifecycle timing (when callbacks are invoked)
- Hard to validate module implementations
- Risk of incorrect lifecycle management (e.g., calling start() before init())

## Evidence
**IModule interface (`components/cdc_core/include/cdc_core/IModule.h`):**
```cpp
class IModule : public IService {
public:
    // Required: Module identification
    virtual const char* getName() const = 0;
    virtual const char* getVersion() const = 0;
    virtual ServiceState getState() const = 0;

    // Required: Lifecycle
    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;

    // Optional: Menu items
    virtual uint8_t getMenuItems(ModuleMenuItem* items, uint8_t maxItems);

    // Optional: TROPIC01 slot allocation
    virtual SlotRequest getSlotRequest() const;
    virtual void setSlotRange(const SlotRange& range);

    // Optional: Event callbacks
    virtual void onUnlock();
    virtual void onLock();
    virtual void onUsbConnect();
    virtual void onUsbDisconnect();
    virtual void onTick(uint32_t nowMs);
};
```

**Missing documentation:**
- When is `init()` called vs `start()`?
- What happens if `init()` returns false?
- Can `start()` be called before `init()` completes?
- What order are `onUnlock()` callbacks fired?
- Can modules call each other directly?
- What is the maximum menu items per module?
- Slot validation timing (before or after init?)

**Current docs (`docs/MODULE_DEVELOPMENT.md:400-430`):**
```
How Module Loading Works
┌─────────────────────────────────────────────────────────────────────┐
│                         BOOT SEQUENCE                               │
...
```
This is a basic sequence diagram but lacks:
- Error handling paths
- Edge cases
- API contract details
- Return value semantics

## Recommended Fix
Add `docs/module-api.md` with:

**1. IModule Interface Reference**
| Method | Required | Called When | Returns | Notes |
|--------|----------|-------------|---------|-------|
| `getName()` | Yes | During registration | Module name string | Must match slot map |
| `init()` | Yes | Boot sequence | true if success | Register services here |
| `start()` | Yes | After all init() | true if success | Start background tasks |
| `stop()` | Yes | Shutdown/refresh | void | Clean up resources |
| `getMenuItems()` | No | Menu rebuild | Count of items | Return 0 for no items |
| `getSlotRequest()` | No | During init | SlotRequest | 0 if no slots needed |
| `onUnlock()` | No | After PIN success | void | One-time per unlock |
| `onTick()` | No | Every ~100ms | void | Use for background work |

**2. Lifecycle State Machine**
```
UNINITIALIZED
    ↓ (init() called)
INITIALIZED
    ↓ (start() called)
STARTED
    ↓ (stop() called)
STOPPED
    ↓ (start() called again)
STARTED
```

**3. Event Callback Timing**
| Callback | Trigger | Guarantees |
|----------|---------|------------|
| `onUnlock()` | PIN success | Called after all modules initialized |
| `onLock()` | PIN lock | Called before UI returns to lock screen |
| `onUsbConnect()` | USB connected | May be called multiple times |
| `onUsbDisconnect()` | USB disconnected | Clean up USB-dependent state |
| `onTick()` | Every ~100ms | Use for periodic work, keep <10ms |

**4. Menu Item Contract**
```cpp
struct ModuleMenuItem {
    const char* label;        // Display text (can be i18n)
    uint8_t priority;         // 0-255, lower = higher in list
    ViewGetter getView;       // Returns IView* or nullptr
    VisibilityCheck isVisible;// Return true if item visible
    const char* moduleName;   // Auto-filled
    MenuLocation location;    // MAIN_MENU, TOOLS_MENU, etc.
};
```

**5. Slot Allocation Contract**
| Slot Type | Range | Allocated By | Notes |
|-----------|-------|--------------|-------|
| ECC | 1-31 | ModuleRegistry | Must be contiguous |
| R-Memory | 32-511 | ModuleRegistry | Must be contiguous |
| Slot 0 | Reserved | System | PIN, attestation |

**6. Error Handling**
- `init()` returns false → Module enters ERROR state, logged
- `start()` returns false → Module stays INITIALIZED, can retry
- Slot validation fails → Module enters ERROR state, user notified

**7. Inter-Module Communication**
- EventBus: `EventBus::instance().publish()`
- Typed services: `ServiceRegistry::instance().request<T>()`
- Direct calls: Not recommended, use events/services

## References
- IModule source: `components/cdc_core/include/cdc_core/IModule.h`
- Module registry: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
- Tutorial guide: `docs/MODULE_DEVELOPMENT.md`
