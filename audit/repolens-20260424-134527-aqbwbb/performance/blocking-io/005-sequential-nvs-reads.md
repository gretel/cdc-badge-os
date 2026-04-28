---
title: "[LOW] Sequential NVS reads in ModuleRegistry loading"
severity: LOW
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "nvs"
  - "modules"
---

## Summary

The `ModuleRegistry::loadDisabledList()` and `ModuleRegistry::cleanupOrphanedModuleData()` functions perform multiple synchronous NVS reads sequentially. While reads are faster than writes, they still block the calling task.

**Affected file:**
- `components/cdc_core/src/ModuleRegistry.cpp` (lines 368-425, 478-490)

**Evidence:**
```cpp
// components/cdc_core/src/ModuleRegistry.cpp:375-379
esp_err_t err = nvs_get_str(handle, MODULES_NVS_KEY, savedList, &len);
nvs_close(handle);

// components/cdc_core/src/ModuleRegistry.cpp:479-485
if (nvs_get_str(handle, MODULES_NVS_KEY_DISABLED, disabledModules_, &len) == ESP_OK) {
    LOG_I(TAG, "Loaded disabled modules: %s", disabledModules_);
}
```

**Called during:**
- Boot (`runAllInitializers()`)
- Module list changes

## Impact

1. **Boot delay**: Multiple NVS reads during startup add to boot time.

2. **Sequential blocking**: Each read blocks until complete before next one starts.

## Evidence

**NVS read operations in ModuleRegistry:**
```cpp
// loadDisabledList() - 1-2 reads
nvs_get_str(handle, MODULES_NVS_KEY_DISABLED, disabledModules_, &len);

// cleanupOrphanedModuleData() - 1 read + potential erases
nvs_get_str(handle, MODULES_NVS_KEY, savedList, &len);
// Then potentially multiple nvs_erase_all() + nvs_commit()
```

## Recommended Fix

1. **Cache module state in RAM**:
   ```cpp
   // Load once at boot, cache in memory
   static char cachedDisabledList[256];
   static bool cacheValid = false;
   
   const char* getDisabledModules() {
       if (!cacheValid) {
           nvs_handle_t handle;
           if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
               size_t len = sizeof(cachedDisabledList);
               nvs_get_str(handle, MODULES_NVS_KEY_DISABLED, cachedDisabledList, &len);
               nvs_close(handle);
               cacheValid = true;
           }
       }
       return cachedDisabledList;
   }
   ```

2. **Batch NVS operations**:
   ```cpp
   // Keep NVS handle open for multiple reads
   nvs_handle_t handle;
   nvs_open(MODULES_NVS_NAMESPACE, NVS_READONLY, &handle);
   
   nvs_get_str(handle, "key1", buf1, &len1);
   nvs_get_str(handle, "key2", buf2, &len2);
   nvs_get_str(handle, "key3", buf3, &len3);
   
   nvs_close(handle);  // Single open/close cycle
   ```

**Estimated effort**: 30 minutes for caching implementation

## References

- NVS read time: ~1-10ms per operation
- NVS handles can be reused for multiple operations
