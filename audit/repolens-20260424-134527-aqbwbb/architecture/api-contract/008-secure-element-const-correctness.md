---
title: "[MEDIUM] ISecureElement interface lacks const-correctness for read operations"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `ISecureElement` interface declares read-only operations but doesn't use `const` qualifiers, allowing them to be called on const instances:

```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h
class ISecureElement : public core::IService {
    // Read operations (should be const)
    virtual SeResult eccGetPublicKey(uint8_t slot, uint8_t* pubKey, EccCurve* curve = nullptr) = 0;
    virtual bool eccSlotUsed(uint8_t slot) const = 0;  // Correct
    virtual SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen, uint16_t* actualLen) = 0;
    virtual bool rmemSlotUsed(uint16_t slot) const = 0;  // Correct
    virtual bool getRandom(uint8_t* buffer, uint16_t size) = 0;
};
```

Some methods have `const` (`eccSlotUsed`, `rmemSlotUsed`) but others don't (`eccGetPublicKey`, `rmemRead`, `getRandom`). This inconsistency means:
- `const ISecureElement* se;` can call `se->eccSlotUsed(0)` but not `se->getRandom(buf, 16)`
- Even though `getRandom` doesn't modify state, it can't be called on a const reference

## Impact
- **Inconsistent API**: Developers must remember which methods are const
- **Limited flexibility**: Cannot pass `const ISecureElement*` to functions that need random numbers
- **Code duplication**: Need separate const/non-const versions

## Evidence
- Interface definition: components/cdc_hal/include/cdc_hal/ISecureElement.h
- Methods with `const`: lines 158, 179
- Methods without `const`: lines 81, 102, 114, 124, 136, 145, 153, 186, 196

## Recommended Fix
Add `const` qualifier to all read-only methods:

```cpp
class ISecureElement : public core::IService {
    // ECC operations
    virtual SeResult eccGetPublicKey(uint8_t slot, uint8_t* pubKey, EccCurve* curve = nullptr) const = 0;
    virtual bool eccSlotUsed(uint8_t slot) const = 0;
    
    // R-Memory operations
    virtual SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen, uint16_t* actualLen) const = 0;
    virtual bool rmemSlotUsed(uint16_t slot) const = 0;
    
    // Random
    virtual bool getRandom(uint8_t* buffer, uint16_t size) const = 0;
    
    // Diagnostics
    virtual bool getChipId(uint8_t* serialNum, uint8_t size) const = 0;
    virtual bool getFwVersion(uint8_t* riscvVer, uint8_t* spectVer) const = 0;
};
```

Update implementations (e.g., `components/usb_badge/src/tropic01/TropicSecureElement.cpp`) to match.

## References
- ISecureElement: components/cdc_hal/include/cdc_hal/ISecureElement.h
