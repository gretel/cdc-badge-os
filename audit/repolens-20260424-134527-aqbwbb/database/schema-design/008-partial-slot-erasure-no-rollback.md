---
title: "[MEDIUM] R-Memory slot erasure without atomic write support"
severity: MEDIUM
domain: database/schema-design
lens: write-atomicity
labels:
  - "storage"
  - "atomicity"
  - "error-handling"
---

## Summary
The R-Memory write operations use a read-modify-write pattern with intermediate erasure, but there's no atomic transaction support. If power is lost between erase and write, the slot becomes corrupted. This affects PIN storage, TOTP accounts, and password entries.

**Evidence:**
- `PinManager.cpp:200-208`:
  ```cpp
  se->rmemErase(RMEM_SLOT_PIN);
  hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);
  if (result != hal::SeResult::OK) {
      LOG_E(TAG, "R-Memory write failed");
      return false;
  }
  ```
  If write fails, slot is empty but data wasn't saved!

- `GpgStorage.cpp:302-312`:
  ```cpp
  // Erase existing data first
  se->rmemErase(rmem_slot);
  // Write encrypted key to R-Memory
  if (se->rmemWrite(rmem_slot, ...) != cdc::hal::SeResult::OK) {
      goto cleanup;
  }
  ```
  Same issue - erase before write with no rollback

- `PasswordStore.cpp:228-238`: Uses `rmemWriteWithHeader()` which likely has same issue

## Impact
1. **Data loss**: Power failure during write leaves slot empty
2. **PIN loss**: If PIN slot is erased but not written, PIN must be reset
3. **Recovery complexity**: No way to detect partial writes
4. **User experience**: Silent data loss is confusing

## Evidence
From `PinManager.cpp:200-208`:
```cpp
se->rmemErase(RMEM_SLOT_PIN);

hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, data, STORAGE_SIZE);
if (result != hal::SeResult::OK) {
    LOG_E(TAG, "R-Memory write failed");
    return false;  // Slot is now empty, data lost!
}
```

From `GpgStorage.cpp:300-312`:
```cpp
// Erase existing data first
se->rmemErase(rmem_slot);

// Write encrypted key to R-Memory
if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
        != cdc::hal::SeResult::OK) {
    LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
    goto cleanup;  // Slot is empty, key lost!
}
```

## Recommended Fix
1. **Implement read-verify-write pattern**: Read old data first, then write new data
2. **Add checksum validation**: Verify data after write
3. **Consider dual-slot approach**: Write to secondary slot, then switch primary
4. **Add recovery logic**: Detect empty slots and prompt for re-initialization

Example pattern:
```cpp
bool saveToStorage() {
    // 1. Read current data
    uint8_t oldData[STORAGE_SIZE];
    se->rmemRead(RMEM_SLOT_PIN, oldData, STORAGE_SIZE, &actualLen);
    
    // 2. Prepare new data
    uint8_t newData[STORAGE_SIZE];
    // ... fill newData ...
    
    // 3. Write to slot
    se->rmemErase(RMEM_SLOT_PIN);
    hal::SeResult result = se->rmemWrite(RMEM_SLOT_PIN, newData, STORAGE_SIZE);
    
    // 4. Verify write
    uint8_t verifyData[STORAGE_SIZE];
    se->rmemRead(RMEM_SLOT_PIN, verifyData, STORAGE_SIZE, &actualLen);
    if (memcmp(newData, verifyData, STORAGE_SIZE) != 0) {
        // Try to restore old data
        se->rmemErase(RMEM_SLOT_PIN);
        se->rmemWrite(RMEM_SLOT_PIN, oldData, STORAGE_SIZE);
        return false;
    }
    return true;
}
```

## References
- EEPROM/Flash wear leveling patterns
- Transactional storage for embedded systems
