---
title: "[LOW] TOTP deleteAccount() lacks slot validation before erase"
severity: LOW
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The `TotpStore::deleteAccount()` function at `components/mod_totp/src/TotpStore.cpp:357-376` erases an R-Memory slot without first verifying that the slot actually contains a TOTP account for the configured module. This could lead to silent failures or erasing wrong data if slot mapping is misconfigured.

## Impact
- **Silent Failures**: Function returns `false` on error but doesn't distinguish between "slot empty", "wrong module", and "erase failed"
- **No Pre-validation**: Doesn't check if slot contains valid TOTP data before erasing
- **Cache Inconsistency**: Calls `eraseSlot()` on cache even if secure element erase fails

## Evidence
File: `components/mod_totp/src/TotpStore.cpp`

Lines 357-376 (deleteAccount):
```cpp
bool TotpStore::deleteAccount(uint16_t slot) {
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemErase(physSlot);  // Erases without validation!
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);  // Updates cache

    return true;
}
```

Compare to `readAccount()` at lines 152-188 which validates module ID:
```cpp
bool TotpStore::readAccount(uint16_t slot, TotpAccount* out) {
    // ...
    auto res = se->rmemReadWithHeader(physSlot, &header, payloadBuf, sizeof(payloadBuf), &payloadLen);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    if (header.moduleId != moduleId_) {  // Validates module!
        return false;
    }
    // ...
}
```

`deleteAccount()` doesn't perform this validation before erasing.

## Recommended Fix
Add validation before erasing:

```cpp
bool TotpStore::deleteAccount(uint16_t slot) {
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;
    
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;
    
    // Validate slot contains TOTP data for this module
    cdc::hal::ISecureElement::RMemHeader header = {};
    uint8_t payloadBuf[64] = {};
    uint16_t payloadLen = 0;
    
    auto res = se->rmemReadWithHeader(physSlot, &header, payloadBuf, sizeof(payloadBuf), &payloadLen);
    if (res != cdc::hal::SeResult::OK) {
        // Slot might be empty - still allow erase to clear it
        LOG_D(TAG, "Slot %u empty, erasing anyway", physSlot);
    } else if (header.moduleId != moduleId_) {
        LOG_W(TAG, "Slot %u has wrong module %u (expected %u)", 
              physSlot, header.moduleId, moduleId_);
        return false;  // Don't erase wrong module's data!
    }
    
    // Now safe to erase
    res = se->rmemErase(physSlot);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);
    return true;
}
```

## References
- TOTP storage: `components/mod_totp/src/TotpStore.cpp`
- Similar pattern in `readAccount()`: `TotpStore.cpp:152`
- Secure element HAL: `components/cdc_hal/ISecureElement.h`
