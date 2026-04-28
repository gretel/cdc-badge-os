---
title: "[MEDIUM] Abbreviation inconsistency: rmem vs RMem vs RMEM"
severity: MEDIUM
domain: cdc_hal
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
The abbreviation for "R-Memory" (TROPIC01 secure element memory) is used inconsistently across the codebase: `rmem` (lowercase), `RMem` (mixed case), `RMEM` (uppercase).

**Evidence**:
```cpp
// ISecureElement.h - uses lowercase rmem for methods
virtual SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                          uint16_t* actualLen) = 0;
virtual SeResult rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) = 0;
virtual SeResult rmemErase(uint16_t slot) = 0;
virtual bool rmemSlotUsed(uint16_t slot) const = 0;
virtual SeResult rmemWriteWithHeader(uint16_t slot, ...);
virtual SeResult rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut, ...);

struct RMemHeader {  // Mixed case here
    uint8_t magic;
    ...
};

// PinManager.h - uses uppercase RMEM
static constexpr uint16_t RMEM_SLOT_PIN = 0;

// IModule.h - uses lowercase rmem
uint16_t rmemStart = 0;
uint16_t rmemEnd = 0;
bool hasRmem = false;

// TROPIC01 docs - use uppercase RMEM
```

## Impact
- **Readability**: Inconsistent abbreviation makes code harder to read
- **Discoverability**: Developers searching for `RMEMRead()` won't find `rmemRead()`
- **Maintainability**: Unclear which form to use for new code

## Evidence
- File: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- File: `components/cdc_core/include/cdc_core/PinManager.h`
- File: `components/cdc_core/include/cdc_core/IModule.h`
- Inconsistent patterns: `rmemRead`, `rmemWrite`, `RMEM_SLOT_PIN`, `RMemHeader`, `hasRmem`

## Recommended Fix
Standardize to one convention. Given that TROPIC01 documentation uses "R-Memory" and the codebase mostly uses lowercase for method names:

```cpp
// Consistent lowercase for methods
virtual SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                          uint16_t* actualLen) = 0;
virtual SeResult rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) = 0;

// Consistent casing for struct name
struct RmemHeader {  // Use Rmem (camelCase-style) not RMem
    uint8_t magic;
    ...
};

// Consistent uppercase for constants
static constexpr uint16_t RMEM_SLOT_PIN = 0;
static constexpr uint16_t RMEM_SLOT_SIZE = 476;

// Consistent lowercase for member variables
uint16_t rmemStart = 0;
uint16_t rmemEnd = 0;
bool hasRmem = false;  // Or hasRmemSlot
```

## References
- TROPIC01 datasheet: Uses "R-Memory" terminology
- C++ Core Guidelines: [C.35: Use a consistent naming style](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C35)
