---
title: "[MEDIUM] Poor section organization in large header files"
severity: MEDIUM
domain: readability
lens: organization
labels:
  - "audit:code-quality/readability"
---

## Summary
Several large header files lack clear section organization, mixing related declarations with unrelated ones. This forces developers to scroll through the entire file to locate specific declarations, rather than being able to quickly navigate to the relevant section.

**Files affected:**
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` (914 lines, no section headers)
- `components/cdc_hal/include/cdc_hal/IKeypad.h` (no clear grouping of related methods)
- `components/mod_password/include/mod_password/PasswordStore.h` (mixed concerns)

## Impact
- **Navigation overhead**: Developers must scan entire files to find relevant declarations
- **Cognitive load**: Related methods are scattered, making it harder to understand the API surface
- **Onboarding friction**: New contributors struggle to understand the file structure

## Evidence

### ModuleRegistry.h - No Section Organization
The `ModuleRegistry.h` header contains ~914 lines with no clear section breaks. Methods are declared in a flat structure without grouping:

```cpp
// Current state - all methods mixed together:
void registerInitializer(ModuleInitFunc initFunc);
bool registerModule(IModule* module);
void unregisterModule(const char* name);
IModule* getModule(const char* name);
IModule* getModuleAt(uint8_t index);
bool initAll();
bool startAll();
bool startModule(uint8_t index);
void stopAll();
uint8_t getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems);
void dispatchUnlock();
void dispatchLock();
void dispatchUsbConnect();
void dispatchUsbDisconnect();
void dispatchTick(uint32_t nowMs);
uint8_t getLockScreenContextItems(LockScreenContextItem* items, uint8_t maxItems);
// ... 20+ more methods without any grouping
```

**What's missing:**
- No `// === Initialization ===` section for `registerInitializer`, `registerModule`, `initAll`
- No `// === Module Lifecycle ===` section for `startAll`, `startModule`, `stopAll`
- No `// === Query Methods ===` section for `getModule`, `getModuleAt`
- No `// === Event Dispatch ===` section for `dispatchUnlock`, `dispatchLock`, `dispatchTick`
- No `// === NVS Persistence ===` section for `saveModuleList`, `loadDisabledList`

### IKeypad.h - Mixed Concerns
```cpp
// Current state - initialization, state, and callbacks mixed:
virtual bool init() = 0;
virtual void setCallback(KeyCallback callback) = 0;
virtual bool start() = 0;
virtual void setLongPressEnabled(bool enabled, uint32_t thresholdMs) = 0;
virtual void stop() = 0;
virtual void setLongPressCallback(LongPressCallback callback) = 0;
// ...
```

**Better organization would be:**
```cpp
// === IService Interface ===
virtual bool init() = 0;
virtual bool start() = 0;
virtual void stop() = 0;
virtual ServiceState getState() const = 0;
virtual const char* getName() const = 0;

// === Key Input ===
virtual void poll() = 0;
virtual Key getNextKey() = 0;
virtual bool hasKey() const = 0;
virtual bool anyKeyDown() const = 0;

// === Callbacks ===
virtual void setCallback(KeyCallback callback) = 0;
virtual void setLongPressCallback(LongPressCallback callback) = 0;

// === Configuration ===
virtual void setLongPressEnabled(bool enabled, uint32_t thresholdMs) = 0;

// === Power Management ===
virtual void prepareForSleep() = 0;
virtual void recoverFromSleep() = 0;
virtual void clearBuffer() = 0;
```

## Recommended Fix

### Step 1: Add Section Headers to ModuleRegistry.h
Insert clear section comments to organize the ~25 public methods into logical groups:

```cpp
class ModuleRegistry {
public:
    // === Singleton Access ===
    static ModuleRegistry& instance();

    // === Module Registration ===
    void registerInitializer(ModuleInitFunc initFunc);
    bool registerModule(IModule* module);
    void unregisterModule(const char* name);

    // === Module Lookup ===
    IModule* getModule(const char* name);
    IModule* getModuleAt(uint8_t index);
    uint8_t getCount() const { return count_; }

    // === Lifecycle Management ===
    bool initAll();
    bool startAll();
    bool startModule(uint8_t index);
    void stopAll();

    // === Event Dispatch ===
    void dispatchUnlock();
    void dispatchLock();
    void dispatchUsbConnect();
    void dispatchUsbDisconnect();
    void dispatchTick(uint32_t nowMs);

    // === Menu Integration ===
    uint8_t getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems);
    uint8_t getLockScreenContextItems(LockScreenContextItem* items, uint8_t maxItems);

    // === Module State ===
    bool isModuleEnabled(uint8_t index) const;
    bool toggleModuleEnabled(uint8_t index);
    void setModuleEnabled(uint8_t index, bool enabled);
    bool hasModuleSlotError(uint8_t index) const;
    const char* getModuleSlotError(uint8_t index) const;

    // === Error Reporting ===
    void reportModuleError(const char* name, const char* message);
    void clearModuleErrorByName(const char* name);
    bool retryModule(uint8_t index);

    // === Slot Validation ===
    bool validateSlotMap(const char* moduleName);
    bool validateEccRange(const char* mapName, const char* moduleName,
                          uint16_t minSlots, IModule::SlotRange& range,
                          uint8_t& moduleId);
    bool validateRmemRange(const char* mapName, const char* moduleName,
                           uint16_t minSlots, IModule::SlotRange& range,
                           uint8_t& moduleId);
    bool applySlotRequest(IModule* module, uint8_t index);

private:
    // === NVS Persistence ===
    void loadDisabledList();
    void saveDisabledList();
    void cleanupOrphanedModuleData();
    void saveModuleList();

    // === Helper Methods ===
    bool isModuleEnabledByName(const char* name) const;
    void setModuleError(uint8_t index, const char* message);
    void clearModuleError(uint8_t index);
    const char* getModuleSlotError(uint8_t index) const;
    void buildSlotErrorMessage(char* buffer, size_t bufSize,
                               const char* errorType, const char* mapName);

    // === Private Data ===
    static constexpr uint8_t MAX_MODULES = 16;
    static constexpr uint8_t MAX_INITIALIZERS = 16;
    IModule* modules_[MAX_MODULES] = {};
    ModuleInitFunc initializers_[MAX_INITIALIZERS] = {};
    // ... rest of private members
};
```

### Step 2: Apply Same Pattern to Other Large Headers
- `IKeypad.h`: Group by IService, Key Input, Callbacks, Configuration, Power Management
- `PasswordStore.h`: Group by Singleton, Slot Management, CRUD Operations, Helpers

### Step 3: Use Consistent Section Header Style
Adopt the project's existing comment style for section headers:
```cpp
// === Section Name ===
```

This matches the style already used in `main.cpp` for boot stages.

## References
- [Google C++ Style Guide - Header Files](https://google.github.io/styleguide/cppguide.html#Header_Files)
- [Clean Code - Organizing Functions](https://cleancodestuff.com/)
- [Microsoft Design Guidelines - Class Organization](https://learn.microsoft.com/en-us/dotnet/standard/design-guidelines/class)
