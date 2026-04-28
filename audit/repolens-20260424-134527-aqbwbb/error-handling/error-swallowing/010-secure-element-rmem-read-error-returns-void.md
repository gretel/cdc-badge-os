---
title: "[MEDIUM] TropicStorage::readChunk() returns void, cannot signal read errors"
severity: MEDIUM
domain: cdc_core
lens: error-handling
labels:
  - "TROPIC01"
  - "R-Memory"
  - "readChunk"
  - "void-return"
---

## Summary
The `TropicStorage::readChunk()` method (in `components/cdc_core/src/TropicStorage.cpp`) returns `void`, making it impossible for callers to know if the R-memory read operation succeeded or failed.

## Impact
Callers of `readChunk()` have no way to detect read failures:
- Corrupted or stale data may be used without the caller knowing
- Missing data appears as zeros without error indication
- Error recovery is impossible since callers don't know something went wrong
- Silent data corruption in dependent modules (GPG, TOTP, Password)

## Evidence
Looking at `TropicStorage` usage patterns, the `readChunk()` method is called without checking for errors. The method signature is:

```cpp
void readChunk(uint16_t slot, uint16_t offset, uint8_t* buffer, uint16_t len);
```

Return type is `void` - no way to signal `SeResult::SLOT_EMPTY`, `SeResult::INVALID_PARAM`, or `SeResult::ERROR`.

Compare to other methods in the same file that correctly return `SeResult`:
```cpp
SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen, uint16_t* actualLen);
SeResult rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut, ...);
```

## Recommended Fix
Change `readChunk()` to return `SeResult` and update all callers:

```cpp
/**
 * \brief Reads chunk of data from R-memory slot.
 * \param slot R-memory slot index.
 * \param offset Byte offset within slot.
 * \param buffer Destination buffer.
 * \param len Number of bytes to read.
 * \return Operation result (OK on success, error code otherwise).
 */
SeResult readChunk(uint16_t slot, uint16_t offset, uint8_t* buffer, uint16_t len) {
    if (!se_) return SeResult::ERROR;
    
    // Read data (may need to handle R-Memory 444-byte chunking)
    // Read from physical slot at offset
    uint8_t data[RMEM_SLOT_SIZE];
    uint16_t actualLen = 0;
    SeResult res = se_->rmemRead(slot, data, RMEM_SLOT_SIZE, &actualLen);
    
    if (res != SeResult::OK) return res;
    
    // Copy requested chunk
    uint16_t available = actualLen - offset;
    uint16_t toCopy = (len < available) ? len : available;
    memcpy(buffer, data + offset, toCopy);
    
    return toCopy == len ? SeResult::OK : SeResult::SLOT_EMPTY;
}
```

Update callers to check return value:
```cpp
SeResult res = storage.readChunk(slot, offset, buffer, len);
if (res != SeResult::OK) {
    // Handle error appropriately
}
```

## References
- TROPIC01 R-Memory specification: 444 bytes per slot
- `ISecureElement` interface: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- `SeResult` enum: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
