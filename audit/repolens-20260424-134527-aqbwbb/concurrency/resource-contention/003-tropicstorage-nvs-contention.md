---
title: "[LOW] TropicStorage NVS operations lack batching, causing potential I/O contention"
severity: LOW
domain: concurrency
lens: resource-contention
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `TropicStorage` class performs individual NVS writes for each slot metadata update without batching. The `setEntry()` method loads a chunk, modifies one entry, and saves it back (line 429-438 in `components/cdc_core/src/TropicStorage.cpp`). When multiple modules update their metadata concurrently, this causes repeated NVS open/read/commit cycles that can contend and block.

```cpp
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;
    entries[offset] = entry;
    return saveChunk(chunkIndex, entries);
}

bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    if (!entries) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    esp_err_t err = nvs_set_blob(nvs, key, entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);  // Blocking commit!
    }
    nvs_close(nvs);
    return err == ESP_OK;
}
```

## Impact
1. **NVS I/O contention**: Each `setEntry()` call opens NVS, reads chunk, writes chunk, commits, and closes. Concurrent calls serialize on NVS.
2. **Write amplification**: `rebuild()` calls `saveChunk()` for every chunk (line 266), potentially 16+ NVS commits in quick succession.
3. **Blocking operations**: `nvs_commit()` can take 10-50ms depending on flash wear leveling.
4. **Module startup delay**: When modules register and write their metadata during startup, they may block each other.

## Evidence
- File: `components/cdc_core/src/TropicStorage.cpp:429-438` (setEntry)
- File: `components/cdc_core/src/TropicStorage.cpp:396-410` (saveChunk)
- File: `components/cdc_core/src/TropicStorage.cpp:218-270` (rebuildVerbose - writes all chunks)
- NVS namespace `tr01_meta` used without persistent handle optimization

## Recommended Fix
Implement a dirty-bit tracking system with deferred commits:

```cpp
class TropicStorage {
private:
    nvs_handle_t nvsHandle_ = 0;  // Persistent handle
    
    // Track dirty chunks (bitmask for 16 chunks)
    uint16_t dirtyChunks_ = 0;
    
    // Batch write timer (commit after 100ms of inactivity)
    esp_timer_handle_t commitTimer_;
    
    // Mark chunk dirty, schedule deferred commit
    bool markChunkDirty(uint16_t chunkIndex) {
        uint16_t bit = 1u << (chunkIndex % 16);
        if (!(dirtyChunks_ & bit)) {
            dirtyChunks_ |= bit;
            // Restart debounce timer
            esp_timer_start_once(commitTimer_, 100000);  // 100us in ns
        }
        return true;
    }
    
    // Timer callback for deferred commit
    static void timerCallback(void* arg) {
        TropicStorage* self = static_cast<TropicStorage*>(arg);
        self->commitDirtyChunks();
    }
    
    bool commitDirtyChunks() {
        if (!nvsHandle_) return false;
        
        for (uint16_t i = 0; i < 16; i++) {
            if (dirtyChunks_ & (1u << i)) {
                char key[12];
                snprintf(key, sizeof(key), "c%u", i);
                nvs_set_blob(nvsHandle_, key, &chunks_[i], sizeof(CacheEntry) * CHUNK_SLOTS);
                dirtyChunks_ &= ~(1u << i);
            }
        }
        nvs_commit(nvsHandle_);
        return true;
    }
};

// Modified setEntry to use batching
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;
    entries[offset] = entry;
    
    // Save to cache
    memcpy(&chunks_[chunkIndex], entries, sizeof(entries));
    
    // Mark dirty, defer commit
    return markChunkDirty(chunkIndex);
}
```

## References
- NVS performance best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#performance
- ESP-Timer for deferred operations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html
