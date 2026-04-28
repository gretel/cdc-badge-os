---
title: "[LOW] TROPIC storage cache uses fixed chunk size without adaptive optimization"
severity: LOW
domain: database
lens: index-strategy
labels:
  - audit:database/index-strategy
---

## Summary
The TROPIC storage cache (`components/cdc_core/src/TropicStorage.cpp`) uses a fixed chunk size of 64 slots (`CHUNK_SLOTS = 64` at line 18) for NVS blob storage. This fixed granularity may not be optimal for all use cases:
- Small modules (like TOTP with 100 slots) span 2 chunks
- Large modules (like Password with 353 slots) span 6 chunks
- Each individual entry update requires loading and saving an entire chunk (64 entries)

**Evidence:**
- `CHUNK_SLOTS = 64` in `components/cdc_core/include/cdc_core/TropicStorage.h:18`
- `setEntry()` at `components/cdc_core/src/TropicStorage.cpp:432-443` loads entire chunk for single update
- `TropicStorage::writeSlot()` calls `setEntry()` which does chunk load/modify/save cycle

## Impact
- **Write amplification**: Single entry update causes 64-entry chunk to be read-modify-written to NVS
- **NVS wear**: More frequent NVS writes than necessary (NVS has limited write endurance)
- **Performance**: Chunk load involves NVS blob read which can be slow for large chunks
- **No tuning**: Chunk size is compile-time constant, not configurable per module

## Evidence
```cpp
// components/cdc_core/include/cdc_core/TropicStorage.h:18
static constexpr uint16_t CHUNK_SLOTS = 64;

// components/cdc_core/src/TropicStorage.cpp:432-443
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;  // Loads 64 entries
    entries[offset] = entry;
    return saveChunk(chunkIndex, entries);  // Saves 64 entries
}
```

```cpp
// For password module with 353 slots:
// - 6 chunks needed (353 / 64 = 5.5)
// - Each write touches one full chunk (64 * sizeof(CacheEntry) bytes)
```

## Recommended Fix
Consider adaptive chunk sizing or write optimization:

1. **Option A: Smaller chunks for frequently-written modules**:
   - Configurable chunk size per module in slot map
   - TOTP/Password could use 16 or 32 slot chunks
   - Less write amplification for smaller, active datasets

2. **Option B: Deferred write coalescing**:
   - Buffer multiple entry updates in memory
   - Write chunk once after N updates or timeout
   - Similar to journaling in databases

3. **Option C: Separate index blob**:
   - Keep sparse index (just used slots) in separate NVS key
   - Full chunk only rebuilt on demand or periodically
   - Reduces writes for sparse datasets

4. **Option D: Bitmask optimization**:
   - Store used-slot bitmask separately (64 bits = 8 bytes)
   - Only load chunk when slot is actually used
   - Skip empty chunks entirely

## References
- NVS write endurance: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#programming-and-erasing
- Write amplification in flash storage: https://en.wikipedia.org/wiki/Write_amplification
- Log-structured merge-tree (LSM) for write optimization: https://en.wikipedia.org/wiki/Log-structured_merge-tree
