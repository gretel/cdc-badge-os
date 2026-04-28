---
title: "[MEDIUM] Duplicate SlotRange type definitions at module boundary"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
Two different `SlotRange` structures are defined at the module boundary between `IModule` and `TropicSlotMap`, creating potential type mismatches and confusion:

1. **`IModule::SlotRange`** (components/cdc_core/include/cdc_core/IModule.h:62-70):
```cpp
struct SlotRange {
    bool hasEcc = false;
    bool hasRmem = false;
    uint8_t eccStart = 0;
    uint8_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t moduleId = 0;
};
```

2. **`TropicSlotMap::SlotRange`** (components/cdc_core/include/cdc_core/TropicSlotMap.h:15-22):
```cpp
struct SlotRange {
    bool valid = false;
    SlotType type = SlotType::ECC;
    const char* moduleName = nullptr;
    uint8_t moduleId = 0;
    uint16_t start = 0;
    uint16_t end = 0;
};
```

The `ModuleRegistry` converts between these two types in `validateEccRange()` and `validateRmemRange()` (ModuleRegistry.cpp:789-860) without explicit conversion functions or shared type definitions.

## Impact
- **Type confusion**: Developers may confuse the two types since they serve related but different purposes
- **Implicit contracts**: The conversion logic is hidden in validation functions, making it hard to trace data flow
- **Maintenance burden**: Changes to one structure may not be reflected in the other
- **No compile-time safety**: Using the wrong type won't cause a compile error since they're different named types in the same namespace

## Evidence
- `IModule::SlotRange`: components/cdc_core/include/cdc_core/IModule.h:62-70
- `TropicSlotMap::SlotRange`: components/cdc_core/include/cdc_core/TropicSlotMap.h:15-22
- Conversion logic: components/cdc_core/src/ModuleRegistry.cpp:789-860
- Usage in modules: components/mod_fido2/src/Fido2Module.cpp:210 (stores `IModule::SlotRange`)

## Recommended Fix
Create a unified slot range definition with clear conversion:

1. Define a shared `SlotAssignment` struct in a common header (e.g., `TropicSlotMap.h`):
```cpp
struct SlotAssignment {
    bool hasEcc;
    bool hasRmem;
    uint8_t eccStart, eccEnd;
    uint16_t rmemStart, rmemEnd;
    uint8_t moduleId;
    const char* moduleName;  // For debugging
};
```

2. Use `SlotAssignment` as the return type for `TropicSlotMap::getRangeByName()` and `getRangeByModuleId()`

3. Update `IModule::SlotRange` to use `SlotAssignment` or alias it:
```cpp
using SlotRange = SlotAssignment;  // In IModule.h
```

4. Add explicit conversion functions:
```cpp
static SlotAssignment fromTropicRange(const TropicSlotMap::SlotRange& ecc, const TropicSlotMap::SlotRange& rmem);
```

## References
- Module interface: components/cdc_core/include/cdc_core/IModule.h
- Slot map implementation: components/cdc_core/src/TropicSlotMap.cpp
- Module registry: components/cdc_core/src/ModuleRegistry.cpp
