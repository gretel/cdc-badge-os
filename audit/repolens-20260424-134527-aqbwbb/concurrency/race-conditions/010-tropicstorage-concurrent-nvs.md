---
title: "[MEDIUM] TropicStorage Concurrent NVS Access Race"
severity: MEDIUM
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `TropicStorage` component (`components/cdc_core/src/TropicStorage.cpp`) performs multiple NVS operations without synchronization. When called concurrently from different modules or tasks, NVS read-modify-write sequences can conflict, leading to cache corruption or lost updates.

**Location**: `components/cdc_core/src/TropicStorage.cpp` (lines 430-460)

## Impact

**Data Corruption**: Concurrent NVS operations can result in:
1. Partial writes (only some chunks updated)
2. Overwritten updates (two writes, only one survives)
3. Inconsistent cache state (header says valid, but chunks are stale)

**Performance**: NVS operations are slow (milliseconds), and concurrent access increases contention and failure rates.

## Evidence

### Read-Modify-Write Pattern in setEntry()

`components/cdc_core/src/TropicStorage.cpp` line 430-442:
```cpp
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;  // READ
    entries[offset] = entry;                            // MODIFY
    return saveChunk(chunkIndex, entries);              // WRITE
}
```

### Race Scenario

Two modules write to different slots in the same chunk:

```
Task A: writeSlot(module1, slot1, "name1", flags1)
        -> calls setEntry(slot1, entry1)
        -> loadChunk(chunk0, entries)  // reads 64 entries
Task B: writeSlot(module2, slot2, "name2", flags2)
        -> calls setEntry(slot2, entry2)
        -> loadChunk(chunk0, entries)  // reads same 64 entries (A's not saved yet)
        -> entries[2] = entry2         // modifies slot 2
        -> saveChunk(chunk0, entries)  // writes 64 entries (slot 1's update lost!)
Task A: -> entries[1] = entry1         // modifies slot 1
        -> saveChunk(chunk0, entries)  // writes 64 entries (overwrites B's update!)
```

Result: Only one module's update survives, even though both wrote to different slots.

### Header-Chunk Inconsistency

`components/cdc_core/src/TropicStorage.cpp` line 167-180:
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

    if (!saveHeader()) {              // Write 1: header
        return false;
    }

    return setEntry(slot, entry);     // Write 2: chunk (could be delayed/interleaved)
}
```

If `saveHeader()` succeeds but `setEntry()` fails (or is interrupted), the cache is inconsistent.

### Concurrent Rebuild

`components/cdc_core/src/TropicStorage.cpp` line 214-280:
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ...
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        memset(chunk, 0, sizeof(chunk));
        // ... read from secure element ...
        if (!saveChunk(chunkIndex, chunk)) {  // Multiple NVS writes
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;
        }
    }

    cacheValid_ = saveHeader();  // Final header write
    return cacheValid_;
}
```

If another task calls `writeSlot()` during rebuild, the updates might be lost or cause corruption.

### NVS Not Thread-Safe

From ESP-IDF documentation: NVS operations are not guaranteed to be atomic across multiple open/close cycles. The `nvs_commit()` ensures durability but not isolation.

## Recommended Fix

### Add NVS Mutex

`components/cdc_core/include/cdc_core/TropicStorage.h`:
```cpp
#include "freertos/semphr.h"

class TropicStorage : public IService {
private:
    // ... existing members ...
    static SemaphoreHandle_t s_nvsMutex;  // Add this
};
```

`components/cdc_core/src/TropicStorage.cpp`:
```cpp
static const char* TAG = "TR01_STORE";
static SemaphoreHandle_t TropicStorage::s_nvsMutex = nullptr;

bool TropicStorage::init() {
    if (state_ != ServiceState::UNINITIALIZED) {
        return state_ == ServiceState::INITIALIZED || state_ == ServiceState::STARTED;
    }

    // Create mutex if not exists
    if (!s_nvsMutex) {
        s_nvsMutex = xSemaphoreCreateMutex();
    }

    header_.version = CACHE_VERSION;
    // ... rest of init ...
}

bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};

    xSemaphoreTake(s_nvsMutex, portMAX_DELAY);
    if (!loadChunk(chunkIndex, entries)) {
        xSemaphoreGive(s_nvsMutex);
        return false;
    }
    entries[offset] = entry;
    bool result = saveChunk(chunkIndex, entries);
    xSemaphoreGive(s_nvsMutex);

    return result;
}

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

    xSemaphoreTake(s_nvsMutex, portMAX_DELAY);
    bool headerOk = saveHeader();
    if (headerOk) {
        // Need to re-load and save chunk within same lock
        uint16_t chunkIndex = slot / CHUNK_SLOTS;
        uint16_t offset = slot % CHUNK_SLOTS;
        CacheEntry entries[CHUNK_SLOTS] = {};
        loadChunk(chunkIndex, entries);
        entries[offset] = entry;
        saveChunk(chunkIndex, entries);
    }
    xSemaphoreGive(s_nvsMutex);

    return headerOk;
}

bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    if (!s_nvsMutex) s_nvsMutex = xSemaphoreCreateMutex();

    xSemaphoreTake(s_nvsMutex, portMAX_DELAY);

    // ... existing rebuild logic ...
    // All saveChunk and saveHeader calls happen while holding mutex

    cacheValid_ = saveHeader();
    xSemaphoreGive(s_nvsMutex);

    return cacheValid_;
}
```

### Alternative: Batch NVS Operations

If performance is critical, batch multiple updates:
```cpp
bool TropicStorage::batchWriteSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    // Collect updates, then write all at once
    // Reduces NVS open/close cycles
}
```

### Consider NVS Transaction Wrapper

Create a helper that ensures atomicity:
```cpp
class NvsTransaction {
public:
    NvsTransaction(const char* namespace_) { nvs_open(namespace_, ...); }
    ~NvsTransaction() { nvs_commit(nvs); nvs_close(nvs); }
    void setBlob(const char* key, const void* data, size_t len);
};
```

## References

- ESP-IDF NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/nvs-flash.html
- NVS performance considerations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/nvs-flash.html#storing-data
- FreeRTOS Mutex: https://www.freertos.org/a00107.html
