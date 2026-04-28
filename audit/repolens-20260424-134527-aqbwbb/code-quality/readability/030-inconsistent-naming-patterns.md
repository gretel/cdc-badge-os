---
title: "[MEDIUM] Inconsistent naming patterns across codebase"
severity: MEDIUM
domain: readability
lens: consistency
labels:
  - "audit:code-quality/readability"
---

## Summary
The codebase exhibits inconsistent naming patterns for similar concepts, requiring developers to remember multiple naming conventions for the same pattern. This inconsistency increases cognitive load and makes code navigation less intuitive.

**Files affected:**
- `components/cdc_core/` - Slot-related methods
- `components/cdc_hal/` - Instance getter methods
- `components/mod_password/` vs `components/mod_totp/` - Similar methods with different names
- `components/cdc_views/` - Callback naming

## Impact
- **Increased cognitive load**: Developers must remember multiple naming patterns
- **Navigation friction**: Harder to find related methods when names don't follow a pattern
- **Bug risk**: Similar methods might be confused due to naming differences

## Evidence

### Issue 1: Slot Mapping Methods - Inconsistent Names

**PasswordStore.cpp** uses `toPhysicalSlot` / `toLogicalSlot`:
```cpp
bool PasswordStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
bool PasswordStore::toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
```

**TotpStore.cpp** uses the same pattern (good):
```cpp
bool TotpStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
bool TotpStore::toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
```

**But ModuleRegistry.cpp** uses different naming:
```cpp
// No consistent pattern - methods scattered across file
bool validateEccRange(...)  // Uses "validate" prefix
bool validateRmemRange(...) // Uses "validate" prefix
bool applySlotRequest(...)  // Uses "apply" prefix
```

**Should be:**
```cpp
// Consistent pattern across all modules:
bool convertToPhysicalSlot(...)
bool convertToLogicalSlot(...)
```

### Issue 2: Instance Getter Methods - Mixed Patterns

**hw_config.h style (for HAL):**
```cpp
IKeypad* getKeypadInstance();
ISecureElement* getSecureElementInstance();
IDisplay* getDisplayInstance();
IPowerManager* getPowerManagerInstance();
ISleepController* getSleepControllerInstance();
```

**But also in same file:**
```cpp
II2cBus* getI2cBus0();  // Uses "0" suffix, not "Instance" suffix
IBluetoothController* getBluetoothControllerInstance();
IWifiController* getWifiControllerInstance();
```

**Inconsistent pattern:**
- `getI2cBus0()` - no "Instance", uses number suffix
- `getKeypadInstance()` - uses "Instance" suffix
- `getBluetoothControllerInstance()` - uses "Instance" suffix

### Issue 3: Module Methods - Different Names for Same Concept

**PasswordStore:**
```cpp
bool addEntry(const PasswordEntry& entry);
bool updateEntry(uint16_t slot, const PasswordEntry& entry);
bool deleteEntry(uint16_t slot);
bool readEntry(uint16_t slot, PasswordEntry* out) const;
bool listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const;
```

**TotpStore:**
```cpp
bool addAccount(const char* name, const char* issuer, ...);
bool updateAccount(uint16_t slot, const char* name, ...);
bool deleteAccount(uint16_t slot);
bool readAccount(uint16_t slot, TotpAccount* out);
// No listAccounts method - uses forEachSlot pattern instead
```

**Inconsistencies:**
- `addEntry` vs `addAccount` - same concept, different noun
- `listEntriesSorted` vs no equivalent in TotpStore
- `readEntry` returns bool, `readAccount` returns void (actually bool)

### Issue 4: Callback Naming - Mixed Patterns

**ListView.h:**
```cpp
using SelectCallback = std::function<void(uint16_t index, void* userData)>;
using ItemRenderer = std::function<bool(...)>;  // Different pattern
```

**TCA9535Keypad.h:**
```cpp
using KeyCallback = std::function<void(Key key, bool pressed)>;
using LongPressCallback = std::function<void(Key key)>;  // Different pattern
```

**IView.h:**
```cpp
using SaveCallback = std::function<void(const char* text)>;  // Different pattern
using SlotRange = struct { ... };  // Not a callback but similar typedef style
```

**Inconsistent patterns:**
- `SelectCallback` - "Callback" suffix
- `ItemRenderer` - no "Callback" suffix
- `KeyCallback` - "Callback" suffix
- `LongPressCallback` - "Callback" suffix
- `SaveCallback` - "Callback" suffix

### Issue 5: Error Handling Methods - Different Patterns

**ModuleRegistry:**
```cpp
void reportModuleError(const char* name, const char* message);
void clearModuleErrorByName(const char* name);
void setModuleError(uint8_t index, const char* message);
void clearModuleError(uint8_t index);
bool hasModuleSlotError(uint8_t index) const;
const char* getModuleSlotError(uint8_t index) const;
```

**Pattern inconsistencies:**
- `reportModuleError` - uses "report" verb
- `clearModuleErrorByName` - uses "clear" verb with "ByName" suffix
- `setModuleError` - uses "set" verb
- `hasModuleSlotError` - uses "has" prefix (predicate style)
- `getModuleSlotError` - uses "get" prefix (getter style)

## Recommended Fix

### Step 1: Standardize Slot Mapping Methods
Choose one pattern and apply everywhere:

**Option A (Verb-first):**
```cpp
// All modules:
bool convertToPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
bool convertToLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
```

**Option B (To-From style):**
```cpp
// All modules:
uint16_t toPhysicalSlot(uint16_t logicalIndex) const;
uint16_t toLogicalSlot(uint16_t slot) const;
```

### Step 2: Standardize Instance Getters
Choose one pattern:

**Option A (Consistent "Instance" suffix):**
```cpp
IKeypad* getKeypadInstance();
ISecureElement* getSecureElementInstance();
IDisplay* getDisplayInstance();
IPowerManager* getPowerManagerInstance();
ISleepController* getSleepControllerInstance();
II2cBus* getI2cBus0Instance();  // Changed from getI2cBus0()
IBluetoothController* getBluetoothControllerInstance();
IWifiController* getWifiControllerInstance();
```

**Option B (No "Instance" suffix, use context):**
```cpp
IKeypad* getKeypad();
ISecureElement* getSecureElement();
IDisplay* getDisplay();
IPowerManager* getPowerManager();
ISleepController* getSleepController();
II2cBus* getI2cBus0();  // Keep as-is (number suffix makes it unique)
IBluetoothController* getBluetoothController();
IWifiController* getWifiController();
```

### Step 3: Standardize Module CRUD Methods
Choose one noun and verb pattern:

**Option A (Entry-focused):**
```cpp
// PasswordStore, TotpStore, etc.:
bool addEntry(...);
bool updateEntry(...);
bool deleteEntry(...);
bool readEntry(...);
bool listEntries(...);
```

**Option B (Account-focused for TOTP, Entry for Passwords):**
```cpp
// Keep separate patterns but document the distinction:
// PasswordStore uses "Entry"
// TotpStore uses "Account"
// Document why: different domain terminology
```

### Step 4: Standardize Callback Typedefs
```cpp
// All callbacks use "Callback" suffix:
using SelectCallback = std::function<void(uint16_t, void*)>;
using ItemRendererCallback = std::function<bool(...)>;  // Added "Callback"
using KeyCallback = std::function<void(Key, bool)>;
using LongPressCallback = std::function<void(Key)>;
using SaveCallback = std::function<void(const char*)>;
```

### Step 5: Standardize Error Handling Methods
```cpp
// Consistent pattern:
void setError(const char* name, const char* message);      // or setModuleError
void clearError(const char* name);                         // or clearModuleError
const char* getError(const char* name);                    // or getModuleError
bool hasError(const char* name);                           // or hasModuleError
void reportError(const char* name, const char* message);   // or reportModuleError
```

### Step 6: Document Naming Conventions
Add to project documentation:
```markdown
## Naming Conventions

### Methods
- Getters: `getSomething()`, `isSomething()`, `hasSomething()`
- Setters: `setSomething()`
- Actions: `addX()`, `removeX()`, `updateX()`, `createX()`, `deleteX()`

### Callbacks
- All use `Callback` suffix: `SelectCallback`, `KeyCallback`, `SaveCallback`

### Instance Getters
- Use `Instance` suffix: `getKeypadInstance()`, `getDisplayInstance()`
- Exception: numbered buses use number suffix: `getI2cBus0()`
```

## References
- [Google C++ Style Guide - Naming](https://google.github.io/styleguide/cppguide.html#Naming)
- [Effective C++ - Item 41: Use consistent naming](https://www.aristeia.com/Book/ECC/2e/)
- [Clean Code - Meaningful Names](https://cleancodestuff.com/)
