---
title: "[LOW] TropicStorage chunk caching could be optimized with read-ahead for sequential access"
severity: LOW
domain: performance/caching
lens: embedded-firmware
labels:
  - "nvs-cache"
  - "read-ahead"
---

## Summary
The `TropicStorage` component (`components/cdc_core/src/TropicStorage.cpp`) caches R-Memory metadata in chunks but loads one chunk at a time. When iterating many slots (e.g., `forEachSlot()`), this can result in repeated NVS open/close operations for sequential chunk access.

**Evidence:**
- File: `components/cdc_core/src/TropicStorage.cpp`
- Chunk size: `CHUNK_SLOTS` (typically 16 slots per chunk)
- Function: `loadChunk()` (line 368-388) opens NVS, reads blob, closes NVS
- Function: `setEntry()` (line 431-442) loads chunk, modifies, saves chunk (two NVS operations)
- `forEachSlot()` (line 87-125) iterates multiple chunks sequentially

## Impact
**Performance Cost:**
- Each `loadChunk()` requires NVS open + get_blob + close (3 NVS operations)
- Each `setEntry()` requires NVS open + get_blob + set_blob + commit + close (5 NVS operations)
- For iterating 100 slots: ~7 chunks → 7 NVS opens/closes just for reading
- NVS open/close overhead: ~2-5ms each

**Sequential Access Pattern:**
- `forEachSlot()` processes chunks sequentially
- Current implementation closes NVS after each chunk
- Could keep NVS handle open for batch operations

## Evidence
From `components/cdc_core/src/TropicStorage.cpp`:

```cpp
/**
 * \brief Loads one cache chunk from NVS into memory.
 */
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);  // Closes after each chunk!

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;
    }
    return true;
}

/**
 * \brief Sets one cache entry by absolute slot index.
 */
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;
    entries[offset] = entry;
    return saveChunk(chunkIndex, entries);  // Opens NVS again!
}
```

Each operation opens/closes NVS independently.

## Recommended Fix
Add batch operation support with NVS handle reuse:

1. **Add batch iteration context**:
```cpp
struct ChunkBatchContext {
    nvs_handle_t nvs;
    uint16_t startChunk;
    uint16_t endChunk;
    CacheEntry chunks[32];  // Cache multiple chunks
    bool valid;
};

static ChunkBatchContext s_batchCtx = {};
```

2. **Add batch load function**:
```cpp
bool TropicStorage::loadChunkBatch(uint16_t startChunk, uint16_t endChunk) {
    if (startChunk > endChunk) return false;
    
    // Close existing batch if different range
    if (s_batchCtx.valid && 
        (startChunk != s_batchCtx.startChunk || endChunk != s_batchCtx.endChunk)) {
        if (s_batchCtx.nvs) nvs_close(s_batchCtx.nvs);
        s_batchCtx.valid = false;
    }
    
    // Open batch if not already open
    if (!s_batchCtx.valid) {
        if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &s_batchCtx.nvs) != ESP_OK) {
            return false;
        }
        s_batchCtx.startChunk = startChunk;
        s_batchCtx.endChunk = endChunk;
        s_batchCtx.valid = true;
    }
    
    // Load all chunks in batch
    for (uint16_t i = startChunk; i <= endChunk; i++) {
        char key[12];
        snprintf(key, sizeof(key), "c%u", i);
        size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
        esp_err_t err = nvs_get_blob(s_batchCtx.nvs, key, &s_batchCtx.chunks[i], &len);
        if (err != ESP_OK) {
            memset(&s_batchCtx.chunks[i], 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        }
    }
    
    return true;
}
```

3. **Add batch close function**:
```cpp
void TropicStorage::closeChunkBatch() {
    if (s_batchCtx.valid && s_batchCtx.nvs) {
        nvs_close(s_batchCtx.nvs);
        s_batchCtx.valid = false;
    }
}
```

4. **Use batch in forEachSlot()**:
```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    // ... existing range calculation ...
    
    uint16_t startChunk = fromSlot / CHUNK_SLOTS;
    uint16_t endChunk = toSlot / CHUNK_SLOTS;
    
    // Use batch loading
    if (!loadChunkBatch(startChunk, endChunk)) {
        return false;
    }
    
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        CacheEntry* entries = &s_batchCtx.chunks[chunk];
        // ... existing processing ...
    }
    
    closeChunkBatch();
    return true;
}
```

## References
- NVS open/close overhead: ~2-5ms each
- NVS blob read: ~1-3ms per chunk (depending on size)
- Batch NVS operations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
