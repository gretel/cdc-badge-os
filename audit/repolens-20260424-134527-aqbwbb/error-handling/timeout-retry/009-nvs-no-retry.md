---
title: "[LOW] NVS blob operations lack retry logic for flash wear leveling delays"
severity: LOW
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "nvs"
  - "flash"
  - "retry"
  - "storage"
---

## Summary
The attestation key service (`components/cdc_core/src/AttestationKeyService.cpp`) and module registry (`components/cdc_core/src/ModuleRegistry.cpp`) perform NVS blob operations without retry logic. Flash wear leveling can cause occasional delays that may cause single-attempt NVS operations to fail.

**Evidence:**

1. File: `components/cdc_core/src/AttestationKeyService.cpp:68-74` - NVS blob read without retry:
   ```cpp
   nvs_handle_t nvs;
   if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
       return false;
   }
   esp_err_t err = nvs_get_blob(nvs, NVS_KEY_PUBHASH, out, &len);
   nvs_close(nvs);
   // Single attempt - no retry on failure
   ```

2. File: `components/cdc_core/src/AttestationKeyService.cpp:86-94` - NVS blob write without retry:
   ```cpp
   nvs_handle_t nvs;
   if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
       return false;
   }
   esp_err_t err = nvs_set_blob(nvs, NVS_KEY_PUBHASH, data, len);
   if (err == ESP_OK) {
       err = nvs_commit(nvs);
   }
   nvs_close(nvs);
   // Single attempt - no retry on failure
   ```

3. File: `components/cdc_core/src/ModuleRegistry.cpp:365-376` - NVS string read without retry:
   ```cpp
   nvs_handle_t handle;
   if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
       return "";
   }
   esp_err_t err = nvs_get_str(handle, MODULES_NVS_KEY, savedList, &len);
   nvs_close(handle);
   // Single attempt - no retry on failure
   ```

4. File: `components/cdc_core/src/ModuleRegistry.cpp:412-418` - NVS string write without retry:
   ```cpp
   nvs_handle_t modHandle;
   if (nvs_open(nsName, NVS_READWRITE, &modHandle) == ESP_OK) {
       esp_err_t err = nvs_set_str(modHandle, "name", name);
       if (err == ESP_OK) {
           nvs_commit(modHandle);
       }
       nvs_close(modHandle);
   }
   // Single attempt - no retry on failure
   ```

## Impact
- **Intermittent failures**: Flash wear leveling can cause occasional NVS delays
- **Data persistence issues**: Critical data like attestation keys may fail to save
- **Module state loss**: Module configuration may not persist across reboots
- **Hard-to-debug issues**: Single-attempt failures appear sporadic

## Recommended Fix
Add retry logic for NVS operations with bounded attempts:

1. Add retry constants:
   ```cpp
   static constexpr uint8_t NVS_RETRY_COUNT = 3;
   static constexpr uint32_t NVS_RETRY_DELAY_MS = 50;
   ```

2. Create retry wrapper for blob operations:
   ```cpp
   bool nvsGetBlobWithRetry(nvs_handle_t handle, const char* key, void* data, size_t* len,
                            uint8_t maxRetries = NVS_RETRY_COUNT) {
       for (uint8_t i = 0; i < maxRetries; i++) {
           esp_err_t err = nvs_get_blob(handle, key, data, len);
           if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
               return true;  // Success or not found (not a retryable error)
           }
           if (i < maxRetries - 1) {
               vTaskDelay(pdMS_TO_TICKS(NVS_RETRY_DELAY_MS));
           }
       }
       return false;
   }
   
   bool nvsSetBlobWithRetry(nvs_handle_t handle, const char* key, const void* data, size_t len,
                            uint8_t maxRetries = NVS_RETRY_COUNT) {
       for (uint8_t i = 0; i < maxRetries; i++) {
           esp_err_t err = nvs_set_blob(handle, key, data, len);
           if (err == ESP_OK) {
               return true;
           }
           if (i < maxRetries - 1) {
               vTaskDelay(pdMS_TO_TICKS(NVS_RETRY_DELAY_MS));
           }
       }
       return false;
   }
   
   bool nvsCommitWithRetry(nvs_handle_t handle, uint8_t maxRetries = NVS_RETRY_COUNT) {
       for (uint8_t i = 0; i < maxRetries; i++) {
           esp_err_t err = nvs_commit(handle);
           if (err == ESP_OK) {
               return true;
           }
           if (i < maxRetries - 1) {
               vTaskDelay(pdMS_TO_TICKS(NVS_RETRY_DELAY_MS));
           }
       }
       return false;
   }
   ```

3. Update attestation key service:
   ```cpp
   bool AttestationKeyService::getPubHash(uint8_t* out, size_t* len) {
       nvs_handle_t nvs;
       if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
           return false;
       }
       bool success = nvsGetBlobWithRetry(nvs, NVS_KEY_PUBHASH, out, len);
       nvs_close(nvs);
       return success;
   }
   
   bool AttestationKeyService::setPubHash(const uint8_t* data, size_t len) {
       nvs_handle_t nvs;
       if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
           return false;
       }
       bool success = nvsSetBlobWithRetry(nvs, NVS_KEY_PUBHASH, data, len);
       if (success) {
           success = nvsCommitWithRetry(nvs);
       }
       nvs_close(nvs);
       return success;
   }
   ```

4. Consider adding NVS initialization check:
   ```cpp
   // Ensure NVS is initialized before first use
   static bool nvsInitialized = false;
   static void ensureNvsInitialized() {
       if (!nvsInitialized) {
           esp_err_t err = nvs_flash_init();
           if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION) {
               // NVS full, erase and retry once
               nvs_flash_erase();
               nvs_flash_init();
           }
           nvsInitialized = true;
       }
   }
   ```

## References
- ESP32 NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- NVS performance considerations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#performance
- Flash wear leveling: https://www.espressif.com/sites/default/files/documentation/esp32-nvs-flash-partition-considerations_en.pdf

</content>