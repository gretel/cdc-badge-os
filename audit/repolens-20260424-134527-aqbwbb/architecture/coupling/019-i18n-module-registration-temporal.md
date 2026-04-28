---
title: "[MEDIUM] I18n module registration has temporal coupling with module initialization"
severity: MEDIUM
domain: architecture/coupling
lens: i18n-registration
labels:
  - "audit:architecture/coupling"
---

## Summary
Modules must register their I18n strings using `I18n::registerModule()` at a specific time during initialization. This registration happens inside module initializers that are called via `ModuleRegistry::runAllInitializers()`. If a module registers strings after another module tries to use them, or if the string base ID is 0 (failure), the module's UI will show raw IDs or crash.

**Evidence:**
- `components/cdc_ui/include/cdc_ui/I18n.h` (lines 254-258): `registerModule()` returns base ID or 0 on failure
- `components/grove_led/src/GroveLedModule.cpp` (lines 55-92): Registers strings in `registerStrings()` function
- `components/grove_led/src/GroveLedModule.cpp` (lines 636-645): Initializer calls `registerStrings()` then registers module

## Impact
**Temporal coupling:** The order of `registerInitializer()` calls matters:
1. `I18n::init()` must be called first (loads language from NVS)
2. All `registerModule()` calls must complete before any `tr()` calls
3. If `registerModule()` returns 0 (failure), all `mstr()` calls return garbage

**Fragile initialization sequence:**
```cpp
// main.cpp (line 227)
ModuleRegistry::instance().runAllInitializers();

// Inside each module initializer (e.g., GroveLedModule.cpp line 636)
cdc::core::ModuleRegistry::instance().registerInitializer([]() {
    auto& moduleReg = cdc::core::ModuleRegistry::instance();
    registerStrings();  // ← Must happen in right order
    moduleReg.registerModule(&GroveLedModule::instance());
});
```

**Failure mode:** If `registerModule()` returns 0:
```cpp
// GroveLedModule.cpp (lines 48-50)
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);  // s_strIdBase = 0 → returns wrong string
}
```

## Evidence
**File: `components/grove_led/src/GroveLedModule.cpp` (lines 28-42)**
```cpp
/** \brief Module-local i18n string offsets. */
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_LEDS = 0;
static constexpr uint16_t STR_GROVE_LED = 1;
static constexpr uint16_t STR_LED_COUNT = 2;
...
```

**File: `components/grove_led/src/GroveLedModule.cpp` (lines 55-62)**
```cpp
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("grove_led", STR_COUNT);

    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;  // ← No error handling, module continues with s_strIdBase = 0
    }
    ...
}
```

**File: `components/cdc_ui/include/cdc_ui/I18n.h` (lines 254-260)**
```cpp
uint16_t registerModule(const char* moduleName, uint16_t count);
```
Returns base ID or 0 on failure (no exception, no throw - silent failure).

## Recommended Fix
**Add validation macro:**
```cpp
// In each module
#define REGISTER_MODULE_STRINGS(moduleName, count) \
    s_strIdBase = i18n.registerModule(moduleName, count); \
    if (s_strIdBase == 0) { \
        LOG_E(TAG, "Failed to register i18n strings for " moduleName); \
        return; \
    } \
    LOG_D(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
```

**Or use runtime assertion:**
```cpp
// In registerStrings()
s_strIdBase = i18n.registerModule("grove_led", STR_COUNT);
ESP_DRAM_ASSERT(s_strIdBase != 0, "I18n module registration failed");
```

**Or defer string lookup:**
Instead of storing `s_strIdBase`, look up strings by name at runtime using a new `I18n::strByName("grove_led", "LED_COUNT")` method.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n interface
- `components/grove_led/src/GroveLedModule.cpp` - Example module registration
- `main/main.cpp` (line 227) - Initialization order
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Initializer registration
