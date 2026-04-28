---
title: "[LOW] TROPIC01 slot map has no runtime validation of module ID uniqueness"
severity: LOW
domain: data-integrity
lens: database
labels:
  - slot-map
  - module-ids
  - compile-time
---

## Summary
The TROPIC01 slot map (`main/tropic_slot_map.h`) defines module IDs as preprocessor macros. While the `TropicSlotMap::validateOnce()` function checks for duplicate module IDs in the slot map entries, the module ID macros themselves could be changed to duplicate values without triggering a compile-time error.

**Files:**
- `main/tropic_slot_map.h:28-35` (module ID definitions)
- `components/cdc_core/src/TropicSlotMap.cpp:122-128` (duplicate check)

## Impact
If a developer modifies `main/tropic_slot_map.h` and accidentally assigns the same module ID to two different modules:
- The compile-time macros won't catch it
- The runtime validation in `TropicSlotMap::validateOnce()` will catch it, but only at boot time
- The error message "slot map module id used by multiple names" is generic

While this is caught at runtime, it would be better to have compile-time uniqueness enforcement.

## Evidence
From `main/tropic_slot_map.h:28-35`:
```cpp
// Module IDs (must be unique, 0-254). 255 is reserved for UNKNOWN.
#define MODULE_ID_MOD_SYSTEM 0
#define MODULE_ID_MOD_GPG 2
#define MODULE_ID_MOD_CA 3
#define MODULE_ID_MOD_FIDO2 4
#define MODULE_ID_MOD_TOTP 5
#define MODULE_ID_MOD_PASSWORD 6
#define MODULE_ID_UNKNOWN 255
```

From `TropicSlotMap.cpp:122-128`:
```cpp
if (a.moduleId == b.moduleId &&
    strcmp(a.moduleName, b.moduleName) != 0) {
    setError("slot map module id used by multiple names");
    return;
}
```

The validation only runs at runtime during `TropicSlotMap` construction.

## Recommended Fix
Add a compile-time uniqueness check using a static assertion pattern:

```cpp
// Module IDs (must be unique, 0-254). 255 is reserved for UNKNOWN.
#define MODULE_ID_MOD_SYSTEM 0
#define MODULE_ID_MOD_GPG 2
#define MODULE_ID_MOD_CA 3
#define MODULE_ID_MOD_FIDO2 4
#define MODULE_ID_MOD_TOTP 5
#define MODULE_ID_MOD_PASSWORD 6
#define MODULE_ID_UNKNOWN 255

// Compile-time uniqueness check using array indexing
#define _TROPIC_CHECK_UNIQUE_ID(id, name) [id] = name
static const char* _tropic_module_names[] = {
    _TROPIC_CHECK_UNIQUE_ID(MODULE_ID_MOD_SYSTEM, "mod_system")
    _TROPIC_CHECK_UNIQUE_ID(MODULE_ID_MOD_GPG, "mod_gpg")
    _TROPIC_CHECK_UNIQUE_ID(MODULE_ID_MOD_CA, "mod_ca")
    _TROPIC_CHECK_UNIQUE_ID(MODULE_ID_MOD_FIDO2, "mod_fido2")
    _TROPIC_CHECK_UNIQUE_ID(MODULE_ID_MOD_TOTP, "mod_totp")
    _TROPIC_CHECK_UNIQUE_ID(MODULE_ID_MOD_PASSWORD, "mod_password")
};
// If two IDs are the same, this will cause "array index out of bounds" or duplicate initialization
```

Or use a simpler approach with a static array and compile-time check:
```cpp
namespace cdc::tropic_map {
    constexpr uint8_t MODULE_IDS[] = {
        MODULE_ID_MOD_SYSTEM,
        MODULE_ID_MOD_GPG,
        MODULE_ID_MOD_CA,
        MODULE_ID_MOD_FIDO2,
        MODULE_ID_MOD_TOTP,
        MODULE_ID_MOD_PASSWORD,
    };
    
    // This will fail to compile if any IDs are duplicated
    constexpr bool check_unique() {
        for (int i = 0; i < sizeof(MODULE_IDS); i++) {
            for (int j = i + 1; j < sizeof(MODULE_IDS); j++) {
                if (MODULE_IDS[i] == MODULE_IDS[j]) return false;
            }
        }
        return true;
    }
    static_assert(check_unique(), "Module IDs must be unique");
}
```

## References
- C++17 `constexpr` for compile-time evaluation
- See `TropicSlotMap.cpp:56-130` for full `validateOnce()` implementation
