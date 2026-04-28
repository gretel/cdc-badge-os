---
title: "[LOW] Inconsistent NVS namespace naming pattern across modules"
severity: LOW
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

The `ModuleRegistry` defines a documented NVS namespace prefix (`"mod_"`) that all modules should use, but the codebase uses **three different patterns** for NVS namespace naming:

1. **Correct pattern**: `"mod_<name>"` (matches ModuleRegistry.NVS_PREFIX)
2. **Short pattern**: `"<name>"` (missing `mod_` prefix)
3. **Compound pattern**: `"mod_<name>"` but defined in different locations

**Files affected:**
- `components/mod_gpg/src/gpg.cpp` - Uses `"mod_gpg"` (correct)
- `components/mod_ble_serial/src/BleSerialModule.h` - Uses `"mod_ble_serial"` (correct)
- `components/mod_hid/src/BleHidKeyboard.cpp` - Uses `"mod_hid"` (correct)
- `components/grove_led/src/GroveLedModule.cpp` - Uses `"mod_grove_led"` (correct)
- `components/mod_gpg/src/openpgp/openpgp.cpp` - Uses `"mod_gpg"` (correct, but separate from gpg.cpp)

However, several core components use abbreviated names without the `mod_` prefix:
- `components/cdc_core/src/AttestationKeyService.cpp` - Uses `"attest"`
- `components/cdc_core/src/TropicStorage.cpp` - Uses `"tr01_meta"`
- `components/cdc_ui/src/I18n.cpp` - Uses `"i18n"`
- `components/cdc_hal/src/SleepController.cpp` - Uses `"sleep"`
- `components/cdc_hal/src/Rtc.cpp` - Uses `"rtc"`
- `components/cdc_hal/src/EpaperDisplay.cpp` - Uses `"display"`

### Documented pattern (ModuleRegistry.h)

```cpp
/**
 * Get the NVS namespace prefix for modules
 * All modules should use "mod_<name>" as their NVS namespace
 */
static constexpr const char* NVS_PREFIX = "mod_";
```

### Correct usage (modules)

```cpp
// In mod_gpg/src/gpg.cpp
static constexpr const char* NVS_NAMESPACE = "mod_gpg";

// In mod_ble_serial/src/BleSerialModule.h
static constexpr const char* NVS_NAMESPACE = "mod_ble_serial";

// In grove_led/src/GroveLedModule.cpp
static constexpr const char* NVS_NAMESPACE = "mod_grove_led";
```

### Abbreviated usage (core components)

```cpp
// In AttestationKeyService.cpp
static constexpr const char* NVS_NAMESPACE = "attest";

// In TropicStorage.cpp
static constexpr const char* NVS_NAMESPACE = "tr01_meta";

// In I18n.cpp
static constexpr const char* NVS_NAMESPACE = "i18n";

// In SleepController.cpp
static constexpr const char* NVS_NAMESPACE = "sleep";
```

## Impact

1. **Documentation mismatch**: ModuleRegistry documents `"mod_"` prefix but core services don't follow it
2. **NVS namespace collisions risk**: Short names like `"sleep"`, `"rtc"`, `"display"` could collide with future modules
3. **Inconsistent naming**: Hard to distinguish module namespaces from core service namespaces when debugging
4. **NVS cleanup complexity**: Tools that iterate NVS namespaces need to handle both patterns

## Evidence

**Modules using correct `"mod_"` prefix:**
- `components/mod_gpg/src/gpg.cpp:3` - `static constexpr const char* NVS_NAMESPACE = "mod_gpg";`
- `components/mod_ble_serial/src/BleSerialModule.h:62` - `static constexpr const char* NVS_NAMESPACE = "mod_ble_serial";`
- `components/mod_hid/src/BleHidKeyboard.cpp:22` - `static constexpr const char* NVS_NAMESPACE = "mod_hid";`
- `components/grove_led/src/GroveLedModule.cpp:18` - `static constexpr const char* NVS_NAMESPACE = "mod_grove_led";`

**Core services using abbreviated names:**
- `components/cdc_core/src/AttestationKeyService.cpp:10` - `static constexpr const char* NVS_NAMESPACE = "attest";`
- `components/cdc_core/src/TropicStorage.cpp:12` - `static constexpr const char* NVS_NAMESPACE = "tr01_meta";`
- `components/cdc_ui/src/I18n.cpp:14` - `static constexpr const char* NVS_NAMESPACE = "i18n";`
- `components/cdc_hal/src/SleepController.cpp:11` - `static constexpr const char* NVS_NAMESPACE = "sleep";`
- `components/cdc_hal/src/Rtc.cpp:12` - `static constexpr const char* NVS_NAMESPACE = "rtc";`
- `components/cdc_hal/src/EpaperDisplay.cpp:14` - `static constexpr const char* NVS_NAMESPACE = "display";`

**Documented pattern:**
- `components/cdc_core/include/cdc_core/ModuleRegistry.h:110` - `static constexpr const char* NVS_PREFIX = "mod_";`

## Recommended Fix

### Option 1: Update documentation (minimal change)

Update the `ModuleRegistry.h` documentation to clarify that:
- **Modules** should use `"mod_<name>"` prefix
- **Core services** can use shorter names (no prefix required)

```cpp
/**
 * Get the NVS namespace prefix for modules
 * Modules should use "mod_<name>" (e.g., "mod_totp", "mod_gpg")
 * Core services use shorter names (e.g., "i18n", "sleep", "rtc")
 */
static constexpr const char* NVS_PREFIX = "mod_";
```

### Option 2: Standardize all namespaces (breaking change)

Update all core services to use a consistent prefix:

```cpp
// Before:
static constexpr const char* NVS_NAMESPACE = "sleep";

// After:
static constexpr const char* NVS_NAMESPACE = "core_sleep";  // or "hal_sleep"
```

This would require:
1. Migrating existing NVS data to new namespaces
2. Updating all `nvs_open()` calls
3. Ensuring backward compatibility during migration

### Option 3: Use namespace categories (recommended)

Define clear categories with distinct prefixes:

```cpp
// Modules: mod_<name>
static constexpr const char* MODULE_NVS_PREFIX = "mod_";

// Core services: core_<name>
static constexpr const char* CORE_NVS_PREFIX = "core_";

// HAL services: hal_<name>
static constexpr const char* HAL_NVS_PREFIX = "hal_";

// Usage:
static constexpr const char* NVS_NAMESPACE = "core_sleep";  // SleepController
static constexpr const char* NVS_NAMESPACE = "hal_display"; // EpaperDisplay
```

## References

- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Documented NVS prefix
- ESP-IDF NVS documentation - Namespace naming conventions
- `components/cdc_core/src/ModuleRegistry.cpp` - Module NVS data cleanup logic

</content>