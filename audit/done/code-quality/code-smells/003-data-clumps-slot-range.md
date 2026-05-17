---
title: "[LOW] Data Clumps: Slot range parameters passed separately instead of as struct"
severity: LOW
domain: core
lens: code-smells
labels:
  - "data-clumps"
  - "cdc_core"
---

## Summary
In `components/cdc_core/`, slot range information (ECC start/end, RMEM start/end, moduleId) is frequently passed as 5 separate parameters instead of as a single `SlotRange` struct. This appears in multiple validation and storage functions.

## Impact
**Parameter ordering errors**: Developers must remember the correct order of 5 related parameters.

**Refactoring difficulty**: Adding a new slot-related parameter requires updating all call sites.

**Readability**: Call sites become hard to read with multiple numeric parameters.

**Inconsistency**: Sometimes `SlotRange` struct is used, sometimes individual parameters.

## Evidence
`components/mod_totp/src/TotpStore.cpp:88-93`:
```cpp
void TotpStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
```

`components/mod_password/src/PasswordStore.cpp:58-63`:
```cpp
void PasswordStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
```

Compare with `IModule::SlotRange` in `components/cdc_core/include/cdc_core/IModule.h:61-69`:
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

The `SlotRange` struct exists but is not consistently used. The `setSlotRange()` methods in stores use separate parameters.

## Recommended Fix
1. **Update `TotpStore::setSlotRange()` signature**:
```cpp
void setSlotRange(const core::IModule::SlotRange& range);
```

2. **Update `PasswordStore::setSlotRange()` signature**:
```cpp
void setSlotRange(const core::IModule::SlotRange& range);
```

3. **Update call sites** in module initialization code to construct `SlotRange` and pass by reference.

**Estimated effort**: ~1 hour to update signatures and fix call sites.

## References
- Refactoring.com: "Data Clumps" - https://refactoring.com/catalog/extractClass
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 6
