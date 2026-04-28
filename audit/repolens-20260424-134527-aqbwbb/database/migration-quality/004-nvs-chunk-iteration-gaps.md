---
title: "[MEDIUM] NVS chunk keys use non-sequential naming causing iteration gaps"
severity: MEDIUM
domain: database/migration-quality
lens: embedded-storage
labels:
  - "nvs-migration"
  - "data-integrity"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp:369-388`, NVS chunk keys are generated using simple string formatting:

```cpp
char key[12];
snprintf(key, sizeof(key), "c%u", chunkIndex);
```

This creates keys like "c0", "c1", "c2", etc. When iterating through chunks during rebuild (line 227-274), the code assumes all chunks from 0 to `totalChunks-1` exist:

```cpp
for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    // ...
    if (!loadChunk(chunkIndex, chunk)) {
        if (logFn) logFn(slotBase, "nvs write failed", ctx);
        return false;
    }
}
```

If a chunk is missing (e.g., due to NVS corruption or partial write), the iteration continues but may process stale data.

## Impact
- **Data inconsistency**: Missing chunks are silently treated as empty (in `loadChunk`, line 369-388), which can lead to incomplete cache rebuilds.
- **No integrity verification**: There's no checksum or count field to verify all expected chunks are present.
- **Sparse data handling**: If chunks are deleted non-sequentially, the system doesn't detect gaps in the data.

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp:369-388`

The `loadChunk` function handles missing data by returning `true` with zeroed entries:
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    // ...
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;  // Missing chunk = empty, but returns success!
    }
    return true;
}
```

In `rebuildVerbose()` (line 217-274), missing chunks don't trigger an error, just a log message if read fails.

## Recommended Fix
Implement robust chunk tracking:

1. **Add chunk count to header**: Store the total number of valid chunks in `CacheHeader`:
   ```cpp
   struct CacheHeader {
       uint8_t version;
       uint8_t chunkSlots;
       uint16_t entrySize;
       uint32_t mapSignature;
       uint16_t totalChunks;  // NEW: Track actual chunk count
   };
   ```

2. **Validate chunk presence**: During rebuild, verify all expected chunks exist:
   ```cpp
   for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
       if (!chunkExists(chunkIndex)) {
           LOG_E(TAG, "Missing chunk %d, rebuild incomplete", chunkIndex);
           return false;
       }
   }
   ```

3. **Add per-chunk checksum**: Include a checksum in each chunk to detect corruption:
   ```cpp
   struct ChunkData {
       uint32_t checksum;
       CacheEntry entries[CHUNK_SLOTS];
   };
   ```

## References
- NVS storage best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/storage/nvs.html
- Data integrity patterns: https://www.cockroachlabs.com/docs/data-integrity
