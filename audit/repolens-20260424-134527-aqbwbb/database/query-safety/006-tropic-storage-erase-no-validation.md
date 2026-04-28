---
title: "[LOW] TropicStorage::eraseSlot() does not validate slot before erase"
severity: LOW
domain: database
lens: query-safety
labels:
  - audit:database/query-safety
---

## Summary
The `TropicStorage::eraseSlot()` function (line 194-203 in `components/cdc_core/src/TropicStorage.cpp`) only checks if the slot is allowed for the module, but does not verify the slot actually contains data before erasing.

**Evidence:**
- File: `components/cdc_core/src/TropicStorage.cpp`
- Lines: 194-203
```cpp
bool TropicStorage::eraseSlot(uint8_t moduleId, uint16_t slot) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    return setEntry(slot, entry);  // Clears cache only, doesn't check if slot has data
}
```

Note: The actual secure element erase happens in calling functions like `PasswordStore::deleteEntry()` and `TotpStore::deleteAccount()`.

## Impact
- **Inefficient Operations**: Erasing empty slots wastes I/O operations
- **No Feedback**: Caller doesn't know if slot was actually populated
- **Potential Race Condition**: Cache might be out of sync with actual slot state

## Recommended Fix
1. Add a check to verify the slot contains data before erasing:
```cpp
bool TropicStorage::eraseSlot(uint8_t moduleId, uint16_t slot) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    if (!getEntry(slot, &entry)) {
        return false;  // Can't read entry, maybe slot doesn't exist
    }

    // Only proceed if slot actually has data
    if (!isEntryUsed(entry)) {
        return true;  // Already empty, nothing to do
    }

    return setEntry(slot, CacheEntry{});
}
```

2. Return a status indicating whether data was actually erased:
```cpp
enum class EraseResult {
    OK,
    EMPTY,      // Slot was already empty
    NOT_FOUND,  // Slot doesn't exist
    FAILED      // Erase failed
};
EraseResult TropicStorage::eraseSlot(uint8_t moduleId, uint16_t slot);
```

## References
- Called by: `PasswordStore::deleteEntry()`, `TotpStore::deleteAccount()`, `GpgStorage`
- Cache lookup: `getEntry()` at line 454
