---
title: "[LOW] TROPIC01 slot map configuration has no validation against module requirements"
severity: LOW
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The slot map configuration (`main/tropic_slot_map.h`) is manually maintained and has no compile-time validation against actual module requirements. Each module declares its requirements via `getSlotRequest()`:

```cpp
// components/mod_gpg/include/mod_gpg/GpgModule.h
core::IModule::SlotRequest getSlotRequest() const override;
```

But there's no mechanism to verify that the slot map matches these requirements at compile time. The validation only happens at runtime in `ModuleRegistry::applySlotRequest()` (components/cdc_core/src/ModuleRegistry.cpp:868-914), which means:

1. Configuration errors are only detected at runtime
2. If a module needs more slots than allocated, it fails silently
3. No compile-time guarantee that all modules can start

Example: GPG module requires 3 ECC slots (components/mod_gpg/src/GpgModule.cpp:618):
```cpp
core::IModule::SlotRequest GpgModule::getSlotRequest() const {
    core::IModule::SlotRequest req = {};
    req.mapName = getName();
    req.minEccSlots = 3;
    req.minRmemSlots = 1;
    return req;
}
```

But the slot map just defines a range:
```cpp
#define ECC_SLOT_MOD_GPG_START 1
#define ECC_SLOT_MOD_GPG_END 3  // This is 3 slots (1, 2, 3)
```

No validation ensures these match.

## Impact
- **Runtime failures**: Slot conflicts discovered at boot time, not compile time
- **Debugging difficulty**: Hard to trace why a module failed to start
- **Configuration drift**: Slot map can diverge from module requirements

## Evidence
- Slot map: main/tropic_slot_map.h
- Module requirements: components/mod_gpg/src/GpgModule.cpp:618
- Runtime validation: components/cdc_core/src/ModuleRegistry.cpp:868-914

## Recommended Fix
Add compile-time slot validation:

1. **Create slot map validation macro** (main/tropic_slot_map.h):
```cpp
#define ASSERT_SLOT_COUNT(name, start, end, expected) \
    static_assert((end - start + 1) == expected, \
                  "Slot map " #name " has " #expected " slots but map has " #end "-" #start)

// Usage:
ECC_SLOT_MOD_GPG_START 1
ECC_SLOT_MOD_GPG_END 3
ASSERT_SLOT_COUNT(mod_gpg_ecc, 1, 3, 3)  // GPG needs 3 slots
```

2. **Or use a build script** to parse both files and validate:
```python
# scripts/validate_slot_map.py
# Parse tropic_slot_map.h and all module getSlotRequest() calls
# Report mismatches at build time
```

3. **Add static assertion in TropicSlotMap** (components/cdc_core/src/TropicSlotMap.cpp):
```cpp
static constexpr uint8_t kExpectedGpgEccSlots = 3;
static constexpr uint8_t kActualGpgEccSlots = 
    ECC_SLOT_MOD_GPG_END - ECC_SLOT_MOD_GPG_START + 1;
static_assert(kActualGpgEccSlots >= kExpectedGpgEccSlots, 
              "GPG slot map too small");
```

## References
- Slot map: main/tropic_slot_map.h
- Module registry: components/cdc_core/src/ModuleRegistry.cpp
- GPG module: components/mod_gpg/src/GpgModule.cpp
