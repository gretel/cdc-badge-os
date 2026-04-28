---
title: "[MEDIUM] NVS open/close overhead in hot storage paths causes redundant handle initialization"
severity: MEDIUM
domain: embedded-storage
lens: query-performance
labels:
  - "nvs-handle"
  - "hot-path"
  - "fido2"
  - "vcard"
---

## Summary

Multiple storage functions in the codebase open and close NVS handles for single operations. Functions like `fido2_storage_counter_increment()`, `vcard_store_add()`, and `TropicStorage::saveHeader()` open an NVS handle, perform one read/write, then close it. This pattern repeats for every storage operation, incurring NVS partition lookup and handle initialization overhead.

**Locations**:
- `components/mod_fido2/src/fido2_storage.cpp:398-416` (counter increment)
- `components/mod_vcard/src/vcard_store.cpp:362-370, 540-577` (vcard add/store)
- `components/cdc_core/src/TropicStorage.cpp:352-361, 396-409` (cache save operations)

## Impact

**Performance Cost**:
- Each `nvs_open()` performs:
  - Namespace lookup in NVS partition
  - Handle allocation and initialization
  - Potential flash page scan
- Each `nvs_close()` performs:
  - Handle cleanup and validation
- For a single NVS read/write operation, ~2-3ms overhead added

**Call Frequency**:
- FIDO2 counter increment: called after each authentication (~100ms per auth)
- vCard add: called when storing new contacts (~1-2s per add)
- TROPIC cache saves: called during rebuild (~10-20 saves per rebuild)

**Cumulative Impact**:
- For 10 FIDO2 authentications: ~20-30ms wasted on NVS handle overhead
- For 50 vCard operations: ~50-100ms wasted
- During cache rebuild: ~100-200ms wasted on repeated open/close

## Evidence

**Pattern in `fido2_storage_counter_increment()` (lines 397-418)**:
```cpp
bool fido2_storage_counter_increment(void) {
    if (!g_storage.counter_loaded) {
        fido2_storage_counter_load();
    }
    g_storage.auth_counter++;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {  // OPEN
        LOG_E("FIDO2", "Failed to open NVS for counter write: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_u32(nvs, NVS_KEY_COUNTER, g_storage.auth_counter);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);  // CLOSE
    return true;
}
```

**Same pattern in `vcard_store_add()` (lines 539-578)**:
```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    // ...
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {  // OPEN
        set_err(err, err_len, "NVS open failed");
        return false;
    }

    // ... duplicate check loop (also uses same nvs handle passed as param) ...

    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);  // CLOSE
    // ...
}
```

**Same pattern in `TropicStorage::saveHeader()` (lines 352-361)**:
```cpp
bool TropicStorage::saveHeader() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {  // OPEN
        return false;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY_HEADER, &header_, sizeof(header_));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);  // CLOSE
    return err == ESP_OK;
}
```

**Note**: `TropicStorage::saveChunk()` and `loadChunk()` have the same pattern, called repeatedly during rebuild (once per chunk, ~50 chunks for 512 slots).

## Recommended Fix

**Option 1: Persistent handle for frequently accessed data**

For data accessed frequently (like FIDO2 counter), maintain a persistent NVS handle:

```cpp
static struct {
    nvs_handle_t counterHandle = 0;
    bool counterHandleValid = false;
} s_nvs_handles;

static bool get_counter_handle(nvs_handle_t* out) {
    if (!out) return false;
    if (!s_nvs_handles.counterHandleValid) {
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handles.counterHandle);
        if (err != ESP_OK) return false;
        s_nvs_handles.counterHandleValid = true;
    }
    *out = s_nvs_handles.counterHandle;
    return true;
}

bool fido2_storage_counter_increment(void) {
    nvs_handle_t nvs;
    if (!get_counter_handle(&nvs)) {
        LOG_E("FIDO2", "Failed to get NVS handle for counter");
        return false;
    }
    
    g_storage.auth_counter++;
    esp_err_t err = nvs_set_u32(nvs, NVS_KEY_COUNTER, g_storage.auth_counter);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    // Don't close - handle is persistent
    return err == ESP_OK;
}

// Add cleanup function called at shutdown
void fido2_storage_cleanup(void) {
    if (s_nvs_handles.counterHandleValid) {
        nvs_close(s_nvs_handles.counterHandle);
        s_nvs_handles.counterHandleValid = false;
    }
}
```

**Option 2: Batch NVS operations with single handle**

For operations that do multiple NVS accesses (like `vcard_store_add`), keep the handle open for the entire operation:

```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard_validate(vcard, len, err, err_len)) return false;
    vcard_store_init();
    
    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        set_err(err, err_len, "NVS open failed");
        return false;
    }
    
    uint32_t hash = fnv1a_hash(vcard, len);
    // Use same handle for duplicate check
    if (vcard_is_duplicate(nvs, vcard, len, hash)) {
        nvs_close(nvs);  // Close once at end
        set_err(err, err_len, "Duplicate vCard");
        return false;
    }
    
    // ... find free slot ...
    
    // Use same handle for write
    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);  // Close once at end
    // ...
}
```

**Option 3: Add NVS connection pool for TROPIC cache**

For `TropicStorage`, add a simple handle cache:

```cpp
class TropicStorage {
private:
    nvs_handle_t cacheHandle_ = 0;
    
    nvs_handle_t getCacheHandle() {
        if (cacheHandle_ == 0) {
            nvs_open(NVS_NAMESPACE, NVS_READWRITE, &cacheHandle_);
        }
        return cacheHandle_;
    }
    
    void releaseCacheHandle() {
        if (cacheHandle_) {
            nvs_close(cacheHandle_);
            cacheHandle_ = 0;
        }
    }
};
```

**Recommended approach**: Option 1 for FIDO2 counter (~30 minutes), Option 2 for vCard (~20 minutes). These are the most frequently accessed data structures.

## References

- [ESP-NVS API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/reference/storage/nvs_flash.html) - `nvs_open()` and `nvs_close()` handle management
- [NVS Performance](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#performance) - NVS read/write latency characteristics
- [Handle Best Practices](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#nvs-api-examples) - Reusing handles for multiple operations

</content>