---
title: "[MEDIUM] Compile-time slot map creates rigid coupling between modules"
severity: MEDIUM
domain: architecture/coupling
lens: slot-map-analysis
labels:
  - "audit:architecture/coupling"
---

## Summary
The TROPIC01 secure element slot allocation is hardcoded in `main/tropic_slot_map.h` and tightly coupled to the `main/CMakeLists.txt` module list. The slot map uses module names (e.g., "mod_gpg", "mod_fido2") that must exactly match runtime module names, creating a fragile compile-time dependency.

**Evidence:**
- `main/tropic_slot_map.h` (lines 38-51): Defines ECC and RMEM slot ranges using `#define` macros
- `main/CMakeLists.txt` (lines 8-20): Lists modules that must match slot map names
- `components/cdc_core/src/TropicSlotMap.cpp` (line 2): Includes `tropic_slot_map.h` directly from `main/`
- `components/mod_gpg/src/GpgModule.cpp` (line 552): Module registers itself and expects slot range from registry

## Impact
**Maintenance burden:** Adding, renaming, or removing a module requires changes in TWO separate places (CMakeLists.txt AND tropic_slot_map.h). If they get out of sync:
- Module may fail to start with "slot range missing" error
- Slot overlap could corrupt another module's data
- No compile-time guarantee of consistency

**Example of fragility:**
```cpp
// tropic_slot_map.h
#define ECC_SLOT_MOD_GPG_START 1
#define ECC_SLOT_MOD_GPG_END 3

// If you rename "mod_gpg" to "gpg" in CMakeLists.txt but forget tropic_map.h:
// → Module registers as "gpg" but slot map looks for "mod_gpg"
// → Module starts with no slot range → silent failure or runtime error
```

## Evidence
**File: `main/tropic_slot_map.h` (lines 55-57)**
```cpp
#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)
```

**File: `main/CMakeLists.txt` (lines 8-20)**
```cmake
set(MODULES
    grove_led
    mod_totp
    mod_fido2
    mod_password
    mod_gpg
    ...
)
```

**File: `components/cdc_core/src/TropicSlotMap.cpp` (lines 18-28)**
```cpp
static const SlotMapEntry kSlotMap[] = {
    TROPIC_ECC_SLOT_MAP(BUILD_ECC_ENTRY)
    TROPIC_RMEM_SLOT_MAP(BUILD_RMEM_ENTRY)
};
```

## Recommended Fix
**Option 1: Derive slot map from CMake (recommended)**
Generate `tropic_slot_map.h` from `main/CMakeLists.txt` using CMake:
```cmake
# In main/CMakeLists.txt
set(SLOT_MAP_HEADER "${CMAKE_CURRENT_BINARY_DIR}/tropic_slot_map.gen.h")
file(WRITE ${SLOT_MAP_HEADER} "// Auto-generated slot map\n")
file(APPEND ${SLOT_MAP_HEADER} "#define TROPIC_ECC_SLOT_MAP(X) \\\n")
foreach(MODULE ${MODULES})
    file(APPEND ${SLOT_MAP_HEADER} "    X(\"${MODULE}\", MODULE_ID_${MODULE}, START, END) \\\n")
endforeach()
```

**Option 2: Runtime validation**
Add runtime check in `ModuleRegistry::runAllInitializers()` to verify all modules in CMake have corresponding slot map entries and vice versa. Log warnings for mismatches.

**Option 3: Single source of truth**
Move slot map definitions into each module's `getSlotRequest()` method. Module registry validates no overlaps at runtime.

## References
- `main/tropic_slot_map.h` - Compile-time slot definitions
- `main/CMakeLists.txt` - Module configuration
- `components/cdc_core/src/TropicSlotMap.cpp` - Slot map runtime validation
- `components/cdc_core/include/cdc_core/IModule.h` - `getSlotRequest()` interface
