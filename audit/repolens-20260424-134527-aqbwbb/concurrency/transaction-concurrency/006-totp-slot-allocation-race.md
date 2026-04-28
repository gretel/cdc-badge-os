---
title: "[LOW] TOTP account findFreeSlot has read-modify-write race on slot allocation"
severity: LOW
domain: transaction-concurrency
lens: cdc-badge-os
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_totp/src/TotpStore.cpp:194-230`, the `findFreeSlot()` function:

```cpp
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    // ...
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    memset(used.get(), 0, cap * sizeof(bool));
    
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            if (candidate <= rmemEnd_) {
                *slotOut = candidate;
                return true;
            }
        }
    }
    return false;
}
```

And `addAccount()` at lines 247-296:
```cpp
bool TotpStore::addAccount(...) {
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        LOG_W(TAG, "No free slots");
        return false;
    }
    // ...
    auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0, &payload, sizeof(payload));
    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);
    return true;
}
```

Two concurrent `addAccount()` calls could:
1. Both call `findFreeSlot()` and get the same slot (e.g., slot 32)
2. Both write to slot 32
3. The second write overwrites the first

## Impact

1. **Lost accounts**: One TOTP account silently overwrites another
2. **Cache-chip mismatch**: The cache may show different slots as used than TROPIC01

## Evidence

**File**: `components/mod_totp/src/TotpStore.cpp`
**Lines**: 194-296

The `findFreeSlot()` function reads from `TropicStorage` cache but doesn't lock the slot for allocation.

## Recommended Fix

This is a low-priority issue since TOTP accounts are typically added one at a time via UI. If needed, add a module-level mutex:

```cpp
static SemaphoreHandle_t s_totp_mutex = nullptr;

bool TotpStore::addAccount(...) {
    if (!s_totp_mutex) {
        s_totp_mutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(s_totp_mutex, portMAX_DELAY);
    
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        xSemaphoreGive(s_totp_mutex);
        LOG_W(TAG, "No free slots");
        return false;
    }
    
    // ... write to TROPIC01 and cache ...
    
    xSemaphoreGive(s_totp_mutex);
    return true;
}
```

## References

- TOTP RFC 6238 specification
- ESP32 FreeRTOS mutex documentation
