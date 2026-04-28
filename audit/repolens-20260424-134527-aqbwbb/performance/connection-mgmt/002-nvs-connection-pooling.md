---
title: "[MEDIUM] NVS connections opened frequently without batching"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp`, the NVS (Non-Volatile Storage) connection is opened and closed for every individual cache chunk read/write operation. This results in repeated NVS handle creation and teardown overhead, especially during cache rebuild operations which iterate through all 512 R-Memory slots in chunks.

**Specific locations:**
- `loadChunk()` (line 373-388): Opens NVS handle, reads one chunk, closes handle
- `saveChunk()` (line 394-408): Opens NVS handle, writes one chunk, closes handle
- `loadHeader()` (line 320-337): Opens NVS handle, reads header, closes handle
- `saveHeader()` (line 340-351): Opens NVS handle, writes header, closes handle

During a cache rebuild (`rebuildVerbose()` at line 211-275), this pattern results in:
- 1 open/close cycle for header
- ~32 open/close cycles for chunks (512 slots / 16 slots per chunk)
- Total: ~33 NVS connection cycles per rebuild

## Impact
1. **Performance overhead**: Each NVS open/close involves namespace lookup, flash sector access, and handle initialization. With ~100ms typical NVS operations, a rebuild takes ~3.3 seconds just in connection overhead.

2. **Flash wear**: Each NVS commit writes to flash. While the current code already batches by chunk, opening the connection multiple times increases the chance of redundant commits.

3. **Blocking time**: NVS operations are synchronous and block the calling task. During `rebuildVerbose()`, the secure element session is held open while waiting for all these NVS operations, extending the critical section.

4. **Scalability**: As the number of slots grows or if this pattern is replicated elsewhere, the overhead compounds.

## Evidence
**File: `components/cdc_core/src/TropicStorage.cpp`**

1. **loadChunk() - opens/closes NVS for each chunk:**
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {  // Open
        return true;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);  // Close

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;
    }
    return true;
}
```

2. **saveChunk() - opens/closes NVS for each chunk:**
```cpp
bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    if (!entries) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {  // Open
        return false;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    esp_err_t err = nvs_set_blob(nvs, key, entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);  // Commit
    }
    nvs_close(nvs);  // Close
    return err == ESP_OK;
}
```

3. **rebuildVerbose() - calls loadChunk/saveChunk in loop:**
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ...
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        memset(chunk, 0, sizeof(chunk));
        // ... read from secure element ...
        if (!saveChunk(chunkIndex, chunk)) {  // Opens and closes NVS each iteration
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;
        }
    }
    // ...
}
```

## Recommended Fix
Implement a connection pooling pattern where the NVS handle is opened once and reused for multiple operations:

1. **Add cached NVS handles:**
```cpp
private:
    nvs_handle_t nvsReadHandle_ = 0;
    nvs_handle_t nvsWriteHandle_ = 0;
    bool nvsHandlesValid_ = false;
```

2. **Add lazy initialization:**
```cpp
bool TropicStorage::ensureNvsHandles() {
    if (nvsHandlesValid_) return true;
    
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvsReadHandle_) != ESP_OK) {
        return false;
    }
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsWriteHandle_) != ESP_OK) {
        nvs_close(nvsReadHandle_);
        return false;
    }
    nvsHandlesValid_ = true;
    return true;
}
```

3. **Add cleanup method:**
```cpp
void TropicStorage::releaseNvsHandles() {
    if (nvsHandlesValid_) {
        nvs_close(nvsReadHandle_);
        nvs_close(nvsWriteHandle_);
        nvsHandlesValid_ = false;
    }
}
```

4. **Update loadChunk() to use pooled handle:**
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    if (!ensureNvsHandles()) return true;
    
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvsReadHandle_, key, entries, &len);

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;
    }
    return true;
}
```

5. **Update saveChunk() to use pooled handle:**
```cpp
bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    if (!entries) return false;
    
    if (!ensureNvsHandles()) return false;
    
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    esp_err_t err = nvs_set_blob(nvsWriteHandle_, key, entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (err == ESP_OK) {
        err = nvs_commit(nvsWriteHandle_);
    }
    return err == ESP_OK;
}
```

6. **Call releaseNvsHandles() in stop():**
```cpp
void TropicStorage::stop() {
    releaseNvsHandles();
    state_ = ServiceState::STOPPED;
}
```

## References
- [ESP-IDF NVS API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html) - NVS connection management
- [NVS Performance Considerations](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html#programming-a-range-of-bits) - Flash write characteristics
- [Connection Pooling Pattern](https://en.wikipedia.org/wiki/Connection_pool) - General pattern
