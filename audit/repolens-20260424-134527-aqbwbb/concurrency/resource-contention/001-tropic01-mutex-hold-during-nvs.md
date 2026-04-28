---
title: "[MEDIUM] TROPIC01 mutex held during NVS operations causing potential blocking"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `Tropic01Element` class in `components/cdc_hal/src/Tropic01Element.cpp` acquires its mutex around TROPIC01 operations, but when these operations trigger NVS writes (via `TropicStorage`), the mutex remains held during potentially slow NVS commit operations. This creates a lock contention hotspot where other threads waiting for the secure element are blocked during NVS I/O.

**Locations:**
- `components/cdc_hal/src/Tropic01Element.cpp:151` - Mutex created
- `components/cdc_hal/src/Tropic01Element.cpp:68-70` - Lock/unlock methods
- `components/cdc_hal/src/Tropic01Element.cpp:583-598` - rmemWrite with lock held
- `components/cdc_core/src/TropicStorage.cpp:398-408` - NVS commit in saveChunk

## Impact
1. **Lock Contention**: The TROPIC01 mutex is a coarse-grained lock protecting all secure element operations. When held during NVS commits (which can take 10-50ms), other threads needing the secure element are blocked.
2. **Priority Inversion**: High-priority tasks (e.g., UI response, keypad input) may be blocked waiting for a low-priority background operation (NVS write).
3. **Scalability**: As more modules access the secure element concurrently, contention on this single mutex increases linearly.

**Evidence:**
```cpp
// Tropic01Element.cpp:rmemWrite (lines 570-599)
SeResult Tropic01Element::rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) {
    // ... parameter validation ...
    
    lock();  // <-- Mutex acquired here
    
    if (!ensureSession("rmemWrite")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }
    
    lt_ret_t ret = lt_r_mem_data_write(&handle_, slot, data, len);
    handleSessionError(ret);
    
    unlock();  // <-- Mutex released here
    return mapResult(ret);
}

// TropicStorage.cpp:writeSlot (lines 168-184)
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    // ... validation ...
    
    CacheEntry entry = {};
    // ... populate entry ...
    
    if (!saveHeader()) {  // <-- NVS commit called
        return false;
    }
    
    return setEntry(slot, entry);  // <-- Another NVS commit
}

// TropicStorage.cpp:saveChunk (lines 396-408)
bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(nvs, key, entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);  // <-- Blocking NVS commit (~10-50ms)
    }
    nvs_close(nvs);
    return err == ESP_OK;
}
```

**Call chain:**
```
Tropic01Element::rmemWrite() [mutex held]
  → TropicStorage::writeSlot()
    → TropicStorage::saveHeader()
      → nvs_commit() [BLOCKING, 10-50ms]
```

## Recommended Fix
Decouple NVS operations from the TROPIC01 mutex by deferring cache updates until after the lock is released:

1. **Option A - Two-phase commit:**
   ```cpp
   SeResult TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
       // Phase 1: Prepare entry data
       CacheEntry entry = {};
       entry.moduleId = moduleId;
       entry.flags = static_cast<uint8_t>(flags | FLAG_USED);
       if (name) {
           strncpy(entry.name, name, sizeof(entry.name) - 1);
           entry.name[sizeof(entry.name) - 1] = '\0';
       }
       
       // Phase 2: Return entry for caller to persist outside lock
       return writeEntry(slot, entry);  // Returns entry, caller commits later
   }
   ```

2. **Option B - Async NVS queue:**
   Create a dedicated NVS write queue that processes commits from a low-priority task, allowing the TROPIC01 mutex to be released immediately after the secure element operation completes.

3. **Option C - Fine-grained locking:**
   Add a separate mutex for NVS cache operations, allowing NVS commits to proceed without blocking TROPIC01 operations:
   ```cpp
   class TropicStorage {
       SemaphoreHandle_t nvsMutex_;  // Separate from TROPIC01 mutex
       // ...
   };
   ```

**Recommended approach:** Option B (async NVS queue) provides the best separation of concerns and minimizes lock contention.

## References
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html) - NVS commit timing
- [FreeRTOS Mutex Best Practices](https://www.freertos.org/Mutexes.html) - Holding locks during I/O
- [Resource Contention Patterns](https://docs.oracle.com/cd/E19455-01/806-0916/6jbof6o5k/index.html) - Lock granularity
