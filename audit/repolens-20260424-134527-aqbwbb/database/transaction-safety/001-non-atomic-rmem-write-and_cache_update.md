---
title: "[HIGH] Non-atomic R-Memory write and cache update in TOTP and Password stores"
severity: HIGH
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/mod_totp/src/TotpStore.cpp` and `components/mod_password/src/PasswordStore.cpp`, multi-step storage operations write to TROPIC01 R-Memory and then update the NVS cache separately without atomic transaction guarantees.

**Affected locations:**
- `TotpStore::addAccount()` (lines 234-293): R-Memory write at line 267, cache update at line 284
- `TotpStore::updateAccount()` (lines 295-358): R-Memory write at line 330, cache update at line 347
- `TotpStore::deleteAccount()` (lines 354-371): R-Memory erase at line 364, cache update at line 368
- `PasswordStore::addEntry()` (lines 203-250): R-Memory write at line 234, cache update at line 250
- `PasswordStore::updateEntry()` (lines 253-298): R-Memory write at line 275, cache update at line 291
- `PasswordStore::deleteEntry()` (lines 301-319): R-Memory erase at line 313, cache update at line 317

## Impact

**Data inconsistency risk:** If the device loses power or crashes between the R-Memory operation and the NVS cache update, the cache and actual storage will be out of sync:

1. **Add operation:** R-Memory slot filled but cache not updated → `forEachSlot()` won't find the new entry
2. **Delete operation:** R-Memory slot erased but cache still marked as used → stale entries appear in listings
3. **Update operation:** New data in R-Memory but old metadata in cache → name/flag mismatches

This creates a "partial commit" scenario where the logical transaction is incomplete.

## Evidence

**TotpStore::addAccount()** (lines 267-284):
```cpp
auto res = se->rmemWriteWithHeader(
    slot,
    moduleId_,
    name,
    0,
    reinterpret_cast<const uint8_t*>(&payload),
    sizeof(payload)
);

if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;  // R-Memory written, but what if crash after this?
}

cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);  // Second write!

return true;
```

**TotpStore::deleteAccount()** (lines 364-368):
```cpp
auto res = se->rmemErase(physSlot);
if (res != cdc::hal::SeResult::OK) {
    return false;
}

cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);  // Second operation!

return true;
```

The same pattern appears in `PasswordStore` with identical structure.

## Recommended Fix

Wrap R-Memory and cache operations in a single atomic transaction. Options:

**Option 1: Batch write with rollback**
```cpp
bool TotpStore::addAccount(const char* name, ...) {
    // ... prepare payload ...
    
    auto* se = cdc::hal::getSecureElement();
    
    // Write to R-Memory first
    auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0, &payload, sizeof(payload));
    if (res != cdc::hal::SeResult::OK) return false;
    
    // Update cache
    bool cacheOk = cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);
    
    // If cache update fails, rollback R-Memory
    if (!cacheOk) {
        se->rmemErase(slot);  // Rollback
    }
    
    return cacheOk;
}
```

**Option 2: Two-phase commit pattern**
Create a helper function in `TropicStorage` that accepts both R-Memory and cache operations:
```cpp
bool TropicStorage::writeSlotWithRmem(
    uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags,
    const uint8_t* rmemData, uint16_t rmemLen,
    cdc::hal::ISecureElement* se
);
```

## References

- ESP32 NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- TROPIC01 R-Memory characteristics (erase-before-write, power-fail safety)
- ACID properties: Atomicity requires all-or-nothing commit