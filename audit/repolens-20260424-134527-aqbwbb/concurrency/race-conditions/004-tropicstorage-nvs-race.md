---
title: "[LOW] TropicStorage NVS Race - Concurrent NVS Access Without Synchronization"
severity: LOW
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `TropicStorage` class in `components/cdc_core/src/TropicStorage.cpp` performs multiple NVS (Non-Volatile Storage) operations that are not synchronized. Multiple modules can access the same NVS namespace concurrently, potentially causing:
1. NVS transaction conflicts
2. Cache corruption during read-modify-write sequences
3. Inconsistent state if one module's write completes between another module's read-modify-write

**Evidence** - `writeSlot()` (lines 167-187):
```cpp
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    entry.moduleId = moduleId;
    entry.flags = static_cast<uint8_t>(flags | FLAG_USED);
    if (name) {
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
    }

    if (!saveHeader()) {  // NVS write - no lock
        return false;
    }

    return setEntry(slot, entry);  // NVS write - no lock
}
```

**Evidence** - `setEntry()` (lines 436-443):
```cpp
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;  // NVS read
    entries[offset] = entry;
    return saveChunk(chunkIndex, entries);  // NVS write
}
```

**Evidence** - `saveChunk()` (lines 396-408):
```cpp
bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    esp_err_t err = nvs_set_blob(nvs, key, entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);  // NVS commit
    }
    nvs_close(nvs);
    return err == ESP_OK;
}
```

**The Race Pattern**:
1. Module A calls `writeSlot(1, slot5, "key1", 0x01)`
2. Module A reads chunk 0 from NVS
3. Module B calls `writeSlot(2, slot10, "key2", 0x01)`
4. Module B reads chunk 0 from NVS (gets same data as A)
5. Module A writes chunk 0 with slot5 updated
6. Module B writes chunk 0 with slot10 updated (overwrites slot5!)

## Impact

**Data Loss**: If two modules write to different slots in the same chunk concurrently, one write can overwrite the other.

**Cache Inconsistency**: The header signature (`computeMapSignature()`) can become stale if multiple writes happen without proper synchronization.

**NVS Wear**: Unnecessary NVS writes due to race conditions can reduce flash lifespan.

## Recommended Fix

Add a mutex to protect NVS operations:

```cpp
// In TropicStorage.h
#include <mutex>

class TropicStorage {
private:
    mutable std::mutex nvsMutex_;  // Add this
    ServiceState state_ = ServiceState::UNINITIALIZED;
    cdc::hal::ISecureElement* secureElement_ = nullptr;
    // ...
};

// In TropicStorage.cpp - protect all NVS operations
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    std::lock_guard<std::mutex> lock(nvsMutex_);
    // ... rest of function
}

bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    std::lock_guard<std::mutex> lock(nvsMutex_);
    // ... rest of function
}

bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    std::lock_guard<std::mutex> lock(nvsMutex_);
    // ... rest of function
}

bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    std::lock_guard<std::mutex> lock(nvsMutex_);
    // ... rest of function
}

// Also protect saveHeader() and loadHeader()
```

Alternatively, use FreeRTOS mutex for consistency:
```cpp
static SemaphoreHandle_t s_nvsMutex = nullptr;

// In init():
s_nvsMutex = xSemaphoreCreateMutex();

// In operations:
xSemaphoreTake(s_nvsMutex, portMAX_DELAY);
// ... NVS operations ...
xSemaphoreGive(s_nvsMutex);
```

## References

- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- NVS transaction safety: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html#transaction-safety
