---
title: "[MEDIUM] TROPIC storage cache update lacks atomicity with secure element reads"
severity: MEDIUM
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/cdc_core/src/TropicStorage.cpp`, the metadata cache is updated without synchronization with the secure element:

```cpp
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    entry.moduleId = moduleId;
    entry.flags = static_cast<uint8_t>(flags | FLAG_USED);
    if (name) {
        strncpy(entry.name, name, sizeof(entry.name) - 1);
    }

    if (!saveHeader()) {
        return false;
    }

    return setEntry(slot, entry);
}
```

The `writeSlot` function updates the NVS cache but doesn't synchronize with the actual TROPIC01 R-Memory write that happens in the calling module (e.g., `TotpStore::addAccount`). If a crash or power loss occurs between the TROPIC01 write and the cache update, the cache becomes stale.

## Impact

1. **Cache inconsistency**: The cache may show slots as used/available differently than the actual secure element state
2. **Rebuild required**: After power cycle, `rebuild()` must be called to synchronize
3. **Duplicate allocation**: If the cache shows a slot as free but it's used in TROPIC01, `findFreeSlot()` may return a slot that's actually in use

## Evidence

**File**: `components/cdc_core/src/TropicStorage.cpp`
**Lines**: 168-186

**File**: `components/mod_totp/src/TotpStore.cpp`
**Lines**: 247-296

The TOTP `addAccount` function:
```cpp
auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0, &payload, sizeof(payload));
if (res != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write slot %u", slot);
    return false;
}

cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);
```

The secure element write happens first, then the cache update. If the cache update fails or is interrupted, the cache is stale.

## Recommended Fix

Make the cache update part of the same transaction. Since NVS and TROPIC01 don't share a transaction mechanism, at minimum:

1. **Update cache first**, then write to TROPIC01
2. **Add verification**: After writing to TROPIC01, verify the cache entry matches

```cpp
bool TotpStore::addAccount(...) {
    // ...
    
    // Update cache FIRST
    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);
    
    // Then write to TROPIC01
    auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0, &payload, sizeof(payload));
    
    // If TROPIC01 write fails, revert cache
    if (res != cdc::hal::SeResult::OK) {
        cdc::core::TropicStorage::instance().eraseSlot(moduleId_, slot);
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }
    
    return true;
}
```

## References

- TROPIC01 datasheet on R-Memory characteristics
- NVS flash wear leveling documentation
