---
title: "[HIGH] Non-atomic read-modify-write in TropicStorage::setEntry causes lost updates under concurrent access"
severity: HIGH
domain: transaction-concurrency
lens: transaction-concurrency
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/cdc_core/src/TropicStorage.cpp`, the `setEntry()` method (lines 430-448) performs a classic read-modify-write pattern across two separate NVS operations without any transactional protection. When multiple tasks concurrently update different entries within the same 64-slot chunk, the second writer silently overwrites the first writer's changes.

**Location:** `components/cdc_core/src/TropicStorage.cpp:430-448`

```cpp
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;
    entries[offset] = entry;
    return saveChunk(chunkIndex, entries);  // Lines 430-448
}
```

The `writeSlot()` method (lines 167-187) calls `setEntry()` after `saveHeader()`, creating a multi-step update:

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

    if (!saveHeader()) {  // Line 184
        return false;
    }

    return setEntry(slot, entry);  // Line 187
}
```

## Impact

**Lost Updates (Write-Write Race):**
1. Task A reads chunk 0 (slots 0-63) into memory
2. Task B reads chunk 0 (slots 0-63) into memory
3. Task A modifies slot 5, writes chunk 0 back to NVS
4. Task B modifies slot 10, writes chunk 0 back to NVS (overwriting slot 5's update)
5. Slot 5's change is silently lost

This affects:
- **PasswordStore** (`components/mod_password/src/PasswordStore.cpp:210-246`): Adding new password entries
- **TotpStore** (`components/mod_totp/src/TotpStore.cpp:245-292`): Adding new TOTP accounts
- **PinManager** (`components/cdc_core/src/PinManager.cpp:155-214`): Saving PIN retry counts (line 327, 428, 529)

**Impact Severity:**
- Data integrity loss for user credentials
- Password entries or TOTP accounts may appear to "disappear"
- PIN retry counters may not reflect actual state, affecting lockout behavior

## Evidence

**TropicStorage::setEntry() - Read-modify-write pattern:**
```cpp
// Line 430-448: components/cdc_core/src/TropicStorage.cpp
bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;  // READ
    entries[offset] = entry;                            // MODIFY
    return saveChunk(chunkIndex, entries);              // WRITE
}
```

**TropicStorage::saveChunk() - NVS write:**
```cpp
// Line 396-410: components/cdc_core/src/TropicStorage.cpp
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
        err = nvs_commit(nvs);  // Commit writes to flash
    }
    nvs_close(nvs);
    return err == ESP_OK;
}
```

**PasswordStore::addEntry() - Uses findFreeSlot + write:**
```cpp
// Line 210-246: components/mod_password/src/PasswordStore.cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {  // READ slot usage
        LOG_W(TAG, "No free password slots");
        return false;
    }
    // ... prepare payload ...
    auto* se = cdc::hal::getSecureElementInstance();
    // ... write to secure element ...
    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, headerName, 0);  // WRITE
    return true;
}
```

**TROPIC01 Element has mutex but doesn't protect cross-call transactions:**
```cpp
// Line 96-97: components/cdc_hal/src/Tropic01Element.cpp
void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }
void unlock() { if (mutex_) xSemaphoreGiveRecursive(mutex_); }
```

The mutex protects individual `rmemWrite()` calls but not the multi-step `rmemWriteWithHeader()` sequence (erase + write) or the TropicStorage read-modify-write.

## Recommended Fix

**Option 1: Add chunk-level mutex protection for TropicStorage**

Add a mutex to `TropicStorage` class and protect the entire read-modify-write sequence:

```cpp
// In TropicStorage.h (add to private section):
#include "freertos/semphr.h"

private:
    // ... existing members ...
    SemaphoreHandle_t chunkMutex_ = nullptr;  // Add this

    // In init() method:
    bool TropicStorage::init() {
        // ... existing init code ...
        chunkMutex_ = xSemaphoreCreateRecursiveMutex();
        if (!chunkMutex_) {
            LOG_E(TAG, "Failed to create chunk mutex");
            return false;
        }
        return true;
    }

    // In setEntry() method:
    bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
        if (!chunkMutex_) return false;
        
        xSemaphoreTakeRecursive(chunkMutex_, portMAX_DELAY);
        
        uint16_t chunkIndex = slot / CHUNK_SLOTS;
        uint16_t offset = slot % CHUNK_SLOTS;
        CacheEntry entries[CHUNK_SLOTS] = {};
        bool result = loadChunk(chunkIndex, entries);
        if (result) {
            entries[offset] = entry;
            result = saveChunk(chunkIndex, entries);
        }
        
        xSemaphoreGiveRecursive(chunkMutex_);
        return result;
    }
```

**Option 2: Use optimistic concurrency with version tracking**

Add a version field to each chunk and verify before write:

```cpp
struct CacheEntry {
    uint8_t moduleId;
    uint8_t flags;
    char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];
    uint8_t version;  // Add version byte
} __attribute__((packed));

// Before save, re-read and compare versions
// If changed, retry or return error
```

**Option 3: Make findFreeSlot + write atomic for PasswordStore/TotpStore**

Add a method to TropicStorage that atomically finds and marks a free slot:

```cpp
bool TropicStorage::findAndAllocateSlot(uint8_t moduleId, uint16_t* slotOut, const char* name) {
    if (!chunkMutex_) return false;
    
    xSemaphoreTakeRecursive(chunkMutex_, portMAX_DELAY);
    
    // Find first free slot in range
    // Immediately mark it as used
    // Write entry
    // Return slot number
    
    xSemaphoreGiveRecursive(chunkMutex_);
    return true;
}
```

## References

1. **ESP-IDF NVS Documentation** - NVS does not provide transaction support across multiple keys: [nvs_flash.md](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
2. **FreeRTOS Semaphores** - For mutex creation: [xSemaphoreCreateRecursiveMutex](https://www.freertos.org/a00127.html)
3. **Lost Update Problem** - Classic concurrency anomaly: [Database Concurrency Anomalies](https://en.wikipedia.org/wiki/Isolation_(database_systems)#Concurrency_anomalies)
4. **TROPIC01 Secure Element** - The secure element itself has no transaction support for multi-slot operations; application-level protection is required.
