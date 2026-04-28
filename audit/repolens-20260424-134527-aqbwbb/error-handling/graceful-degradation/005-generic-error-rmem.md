---
title: "[LOW] R-Memory Operations Return Generic ERROR Without Distinguishing Recovery Scenarios"
severity: LOW
domain: secure-element
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp`, R-Memory read operations return generic `SeResult::ERROR` for various failure modes (header validation, partial data, checksum mismatch). This makes it impossible for callers to distinguish between recoverable errors (e.g., corrupted entry - try next slot) vs. unrecoverable errors (e.g., hardware failure).

Lines of interest:
- `Tropic01Element.cpp:730-762` - `rmemReadWithHeader()` returns generic `SeResult::ERROR` for multiple failure modes
- `TropicStorage.cpp` - Cache operations use generic error returns

## Impact
Generic error returns cause:
1. **No granular error handling** - callers treat all errors the same way
2. **Lost opportunity for partial data** - e.g., if one R-Memory slot is corrupted, try next slot
3. **No retry strategies** - e.g., checksum error could be retried (transient read error)
4. **Debugging difficulty** - hard to diagnose root cause of failures

## Evidence
```cpp
// Tropic01Element.cpp:730-762
SeResult Tropic01Element::rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                              uint8_t* payloadOut, uint16_t payloadMax,
                                              uint16_t* payloadLenOut) {
    // ...
    SeResult res = rmemRead(slot, buffer, sizeof(buffer), &actualLen);
    if (res != SeResult::OK) {
        return res;  // Could be SLOT_EMPTY, SESSION_REQUIRED, or ERROR
    }
    if (actualLen < sizeof(RMemHeader)) {
        return SeResult::ERROR;  // Should be INVALID_DATA or PARTIAL
    }
    RMemHeader header = {};
    memcpy(&header, buffer, sizeof(header));
    if (!validateHeader(header)) {
        return SeResult::ERROR;  // Should be CHECKSUM_MISMATCH
    }
    // ...
    if (actualLen < static_cast<uint16_t>(sizeof(RMemHeader) + header.payloadLen)) {
        return SeResult::ERROR;  // Should be PARTIAL_DATA
    }
    // ...
}
```

## Recommended Fix
Add granular error types for R-Memory operations:

1. **Extend SeResult enum**:
   ```cpp
   enum class SeResult : uint8_t {
       OK,
       ERROR,              // Generic hardware error
       SESSION_REQUIRED,
       SLOT_EMPTY,
       SLOT_OCCUPIED,
       INVALID_PARAM,
       ALARM_MODE,
       NOT_SUPPORTED,
       CHECKSUM_MISMATCH,  // New: header checksum failed
       PARTIAL_DATA,       // New: incomplete read
       INVALID_HEADER,     // New: magic mismatch
       RETRY_RECOMMENDED   // New: transient error, try again
   };
   ```

2. **Update error returns**:
   ```cpp
   if (!validateHeader(header)) {
       if (header.magic != RMEM_HEADER_MAGIC) {
           return SeResult::INVALID_HEADER;
       }
       return SeResult::CHECKSUM_MISMATCH;
   }
   if (actualLen < static_cast<uint16_t>(sizeof(RMemHeader) + header.payloadLen)) {
       return SeResult::PARTIAL_DATA;
   }
   ```

3. **Add retry logic for transient errors**:
   ```cpp
   // In TropicStorage
   bool TropicStorage::loadChunk(uint16_t chunk, CacheEntry* entries) {
       SeResult res = rmemReadWithHeader(...);
       if (res == SeResult::CHECKSUM_MISMATCH) {
           // Try next chunk or return cached data
           return loadFromCache(chunk, entries);
       }
       // ...
   }
   ```

## References
- [Error Code Design Patterns](https://martinfowler.com/bliki/ErrorCodesWorthIt.html)
- [TROPIC01 R-Memory Specification](https://www.microchip.com/en-us/products/security-ic/tropic01)
