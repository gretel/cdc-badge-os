---
title: "[CRITICAL] TROPIC01 Secure Element integration lacks tests for ECC/R-Memory operations"
severity: CRITICAL
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_hal"
  - "area:secure-element"
---

## Summary
The TROPIC01 secure element is the core hardware feature providing ECC key storage and signing. The `ISecureElement` interface and `TropicStorage` cache layer have **no integration tests** verifying actual hardware operations (key generation, signing, R-Memory read/write).

## Impact
- **Critical security feature untested**: ECC signing, key storage could fail silently
- **Slot map validation**: Overlapping slot ranges could corrupt data
- **Cache consistency**: TropicStorage cache may not match actual hardware state
- **Session management**: sessionStart/sessionEnd not verified

## Evidence

**ISecureElement API** (`components/cdc_hal/include/cdc_hal/ISecureElement.h:43-200`):
```cpp
class ISecureElement {
    bool sessionStart();
    SeResult eccGenerate(uint8_t slot, EccCurve curve);
    SeResult ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen, uint8_t* sig, size_t* sigLen);
    SeResult rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len);
    SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen, uint16_t* actualLen);
};
```

**TropicStorage API** (`components/cdc_core/include/cdc_core/TropicStorage.h:20-48`):
```cpp
class TropicStorage {
    bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);
    bool writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags);
    bool rebuild();
};
```

**TropicSlotMap** (`components/cdc_core/include/cdc_core/TropicSlotMap.h:6-45`):
```cpp
class TropicSlotMap {
    bool getRangeByName(const char* moduleName, SlotType type, SlotRange* out);
    bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId);
};
```

**Slot allocation** (`main/tropic_slot_map.h`):
- GPG: ECC slots 1-3, R-Memory slots 1-3
- FIDO2: ECC slots 5-31, R-Memory slots 5-31
- TOTP: R-Memory slots 32-131
- Password: R-Memory slots 150-511

**Current test coverage**: None

## Recommended Fix

Create integration test `test_tropic_secure_element/` that verifies:

1. **Session management**: sessionStart/sessionEnd works correctly
2. **ECC key operations**: Generate, import, get public key, delete
3. **Signing operations**: ECDSA and EdDSA signing
4. **R-Memory operations**: Write, read, erase, slot validation
5. **Slot map constraints**: Modules only access their assigned slots
6. **TropicStorage cache**: Cache rebuilds correctly after hardware changes

**Test structure** (example):
```cpp
// test/test_tropic_secure_element/test_ecc_operations.cpp
#include "cdc_hal/ISecureElement.h"

void test_ecc_generate_and_sign() {
    ISecureElement* se = getSecureElementInstance();
    se->init();
    se->start();
    
    ASSERT_TRUE(se->sessionStart());
    
    // Generate key
    SeResult result = se->eccGenerate(1, EccCurve::P256);
    ASSERT_EQ(result, SeResult::OK);
    
    // Sign hash
    uint8_t hash[32] = {0};
    uint8_t sig[64];
    size_t sigLen;
    result = se->ecdsaSign(1, hash, 32, sig, &sigLen);
    ASSERT_EQ(result, SeResult::OK);
    ASSERT_EQ(sigLen, 64);
    
    se->sessionEnd();
}

void test_rmem_slot_constraints() {
    ISecureElement* se = getSecureElementInstance();
    TropicSlotMap& slotMap = TropicSlotMap::instance();
    
    // GPG should only access slots 1-3
    TropicSlotMap::SlotRange range;
    slotMap.getRangeByName("mod_gpg", TropicSlotMap::SlotType::RMEM, &range);
    ASSERT_EQ(range.start, 1);
    ASSERT_EQ(range.end, 3);
}
```

## References
- [ISecureElement interface](components/cdc_hal/include/cdc_hal/ISecureElement.h)
- [TropicStorage cache](components/cdc_core/include/cdc_core/TropicStorage.h)
- [TropicSlotMap](components/cdc_core/include/cdc_core/TropicSlotMap.h)
- [Slot map definition](main/tropic_slot_map.h)
