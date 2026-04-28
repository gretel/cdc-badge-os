---
title: "[MEDIUM] TropicStorage::loadChunk silently truncates partial NVS reads"
severity: MEDIUM
domain: cdc_core/TropicStorage
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `TropicStorage::loadChunk()` (file: `components/cdc_core/src/TropicStorage.cpp:368-388`), when NVS reads a blob that is smaller than expected, the function silently returns `true` with zeroed entries. This can hide data corruption or partial writes.

Lines 368-388:
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;  // Return true with zeros
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;  // Return true even on partial read!
    }
    return true;
}
```

When `len != sizeof(CacheEntry) * CHUNK_SLOTS`, the function:
1. Zeroes the entries
2. Returns `true` (success)

This masks partial reads where only some data was loaded.

## Impact
- **Silent data loss**: Partial reads appear successful
- **Cache inconsistency**: Some entries may be lost without detection
- **Debugging difficulty**: Hard to distinguish between empty and corrupted chunks

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`, lines 368-388

```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;  // Line 377
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;  // Line 384 - returns success even on error
    }
    return true;
}
```

The function is called from `forEachSlot()` at line 110:
```cpp
for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
    if (!loadChunk(chunk, entries)) {
        return false;
    }
    // Process entries...
}
```

Since `loadChunk()` returns `true` on both success and "partial read", the caller cannot distinguish between them.

## Recommended Fix
Return `false` on partial reads and add logging:

```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        // Chunk not found - return true with zeros (normal for empty cache)
        return true;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err != ESP_OK) {
        // Error reading - return true with zeros
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;
    }
    
    if (len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        // Partial read - this is a problem!
        LOG_W(TAG, "Chunk %u: partial read (got %zu, expected %zu)", 
              chunkIndex, len, sizeof(CacheEntry) * CHUNK_SLOTS);
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return false;  // Return false to signal partial read
    }
    
    return true;
}
```

Also update callers to handle partial reads:
```cpp
for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
    if (!loadChunk(chunk, entries)) {
        LOG_E(TAG, "Failed to load chunk %u, stopping iteration", chunk);
        return false;
    }
    // Process entries...
}
```

## References
- CWE-20: Improper input validation
- CWE-457: Use of uninitialized data
