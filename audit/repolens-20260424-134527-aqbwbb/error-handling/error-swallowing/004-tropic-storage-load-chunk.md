---
title: "[LOW] TropicStorage::loadChunk() returns true on NVS error masking data issues"
severity: LOW
domain: cdc_core
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp:369-388`, the `loadChunk()` function returns `true` even when NVS read fails, silently filling the buffer with zeros. This masks potential data corruption or NVS issues.

**Location:** `components/cdc_core/src/TropicStorage.cpp:369-388`

```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;  // Returns true even on error!
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;  // Returns true even on error!
    }
    return true;
}
```

## Impact
- Callers cannot distinguish between "chunk not yet stored" and "NVS read failed"
- Data corruption in NVS may go undetected
- Cache consistency may be compromised without indication
- Debugging storage issues becomes harder

## Evidence
The function returns `true` in all cases:
1. Line 375: `nvs_open()` fails → returns `true`
2. Line 385: `nvs_get_blob()` fails → returns `true`
3. Line 387: Success → returns `true`

The comment says "missing chunks return zeroed entries and `true`" but this conflates "not found" with other errors like NVS corruption or full storage.

## Recommended Fix
Differentiate between "not found" (OK) and actual errors:

```cpp
/**
 * \brief Loads one cache chunk from NVS into memory.
 * \param chunkIndex Chunk index to load.
 * \param entries Destination buffer for `CHUNK_SLOTS` entries.
 * \return `true` on success or if chunk not yet stored (zeroed entries);
 *         `false` on NVS error (corruption, full, etc.)
 */
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        LOG_D(TAG, "NVS open failed for chunk load: %s", esp_err_to_name(err));
        return false;  // Real error
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Chunk not yet stored - this is OK, return zeroed entries
        return true;
    }
    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        LOG_W(TAG, "Chunk %u load failed: %s (len=%zu)", chunkIndex, esp_err_to_name(err), len);
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return false;  // Real error
    }
    return true;
}
```

## References
- ESP-IDF NVS Error Codes: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#errors
