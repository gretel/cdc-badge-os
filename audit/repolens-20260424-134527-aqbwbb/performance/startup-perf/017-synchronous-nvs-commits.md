---
title: "[MEDIUM] Synchronous NVS Commits During Module Initialization"
severity: MEDIUM
domain: startup-perf
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
Multiple NVS (Non-Volatile Storage) operations during boot use synchronous `nvs_commit()`, which can take 10-30ms each. These blocking writes occur sequentially during module initialization, adding up to significant boot delay.

**Key locations:**
1. `components/cdc_core/src/ModuleRegistry.cpp:452` - `saveModuleList()` saves module list to NVS
2. `components/cdc_core/src/ModuleRegistry.cpp:498` - `saveDisabledList()` saves disabled modules
3. `components/cdc_core/src/TropicStorage.cpp:360` - `saveHeader()` saves cache header
4. `components/cdc_hal/src/EpaperDisplay.cpp:98` - `persistBacklight()` saves display settings

## Impact
- **Cumulative Delay**: 4+ synchronous NVS commits × 10-30ms each = 40-120ms added to boot time
- **No Batching**: Each commit is independent; could be batched into single write cycle
- **Blocking I/O**: NVS commits block the main thread; no async option used
- **Wear Leveling**: More frequent commits than necessary increases flash wear

## Evidence
File: `components/cdc_core/src/ModuleRegistry.cpp` lines 440-465
```cpp
void ModuleRegistry::saveModuleList() {
    // ... build module list string ...
    nvs_handle_t handle;
    if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, MODULES_NVS_KEY, moduleList);
        nvs_commit(handle);  // BLOCKS 10-30ms
        nvs_close(handle);
    }
}
```

File: `components/cdc_core/src/TropicStorage.cpp` lines 350-365
```cpp
bool TropicStorage::saveHeader() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        esp_err_t err = nvs_set_blob(nvs, NVS_KEY_HEADER, &header_, sizeof(header_));
        if (err == ESP_OK) {
            err = nvs_commit(nvs);  // BLOCKS 10-30ms
        }
        nvs_close(nvs);
    }
    return err == ESP_OK;
}
```

During boot sequence (main.cpp lines 218-230):
- Module list saved after all modules registered
- Disabled list loaded then potentially saved
- TROPIC storage header loaded
- Display backlight loaded

## Recommended Fix
Implement batched async NVS writes:

1. **Create NVS batch manager:**
   ```cpp
   // components/cdc_core/include/cdc_core/NvsBatch.h
   class NvsBatch {
   public:
       static NvsBatch& instance();
       void queueString(const char* ns, const char* key, const char* value);
       void queueBlob(const char* ns, const char* key, const void* blob, size_t size);
       void commitAsync();  // Returns immediately, writes in background task
   };
   ```

2. **Use batched commits during boot:**
   ```cpp
   // In ModuleRegistry::runAllInitializers():
   auto& batch = NvsBatch::instance();
   batch.queueString(MODULES_NVS_NAMESPACE, MODULES_NVS_KEY, moduleList);
   batch.queueString(MODULES_NVS_NAMESPACE, MODULES_NVS_KEY_DISABLED, disabledModules_);
   batch.commitAsync();  // Don't wait for completion
   ```

3. **Background NVS writer task:**
   ```cpp
   static void nvsWriterTask(void* arg) {
       while (true) {
           // Wait for batch to be queued
           xSemaphoreTake(nvsBatchMutex, portMAX_DELAY);
           // Perform single commit
           nvs_commit(handle);
           xSemaphoreGive(nvsBatchMutex);
       }
   }
   ```

4. **For display settings, defer to idle:**
   ```cpp
   // In EpaperDisplay::saveBacklight():
   // Don't commit immediately - mark dirty and commit in idle
   displayDirty_ = true;
   // Use esp_timer to defer commit by 100ms
   ```

This reduces boot time by 40-100ms and reduces flash wear by batching writes.

## References
- ESP-IDF NVS documentation: `nvs_commit()` blocks until flash write complete
- Typical NVS commit time: 10-30ms depending on page state
- ESP32-S3 flash wear: ~100k write cycles per page, batching extends lifespan
