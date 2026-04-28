---
title: "[LOW] ESP_ERROR_CHECK used for non-critical operations"
severity: LOW
domain: error-path-tests
lens: error-handling
labels:
  - "esp_error_check"
  - "error-handling"
  - "initialization"
---

## Summary
In `main/main.cpp` (line 59, 62) and `components/CalEPD/epdspi.cpp` (line 73), `ESP_ERROR_CHECK()` is used for initialization operations that might reasonably fail. This causes the system to abort on errors that could potentially be recovered from or logged.

**Files:**
- `main/main.cpp:59, 62`
- `components/CalEPD/epdspi.cpp:73`

## Impact
1. **Hard Crashes**: Non-critical failures cause system reset
2. **No Graceful Degradation**: System can't continue with limited functionality
3. **Debug Difficulty**: Crash logs may not show which operation failed
4. **Boot Loops**: If error is transient, system repeatedly crashes

## Evidence
From `main/main.cpp`:

```cpp
// Line 58-62: NVS flash initialization
void setupNvs() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS table was corrupted, erase it
        ESP_ERROR_CHECK(nvs_flash_erase());  // ❌ Aborts if erase fails!
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);  // ❌ Aborts if init fails!
}

// Line 71-74: USB CDC init - uses ESP_ERROR_CHECK
ESP_ERROR_CHECK(usb_cdc_init());  // ❌ Aborts if USB fails!
```

From `components/CalEPD/epdspi.cpp`:

```cpp
// Line 67-73: SPI bus initialization
esp_err_t ret = spi_bus_initialize(HSPI_PORT, &spi_default_cfg, 1);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI init failed: %s", esp_err_to_name(ret));
}
ESP_ERROR_CHECK(ret);  // ❌ Aborts even though error was logged!
```

**Problem:**
- `ESP_ERROR_CHECK()` calls `abort()` on failure
- Some failures are recoverable (e.g., NVS corruption)
- System should continue with degraded mode or at least log better

## Recommended Fix
Replace `ESP_ERROR_CHECK()` with proper error handling:

1. **Fix NVS initialization**:
   ```cpp
   void setupNvs() {
       esp_err_t ret = nvs_flash_init();
       if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
           // NVS table was corrupted, erase it
           ret = nvs_flash_erase();
           if (ret != ESP_OK) {
               LOG_E("NVS", "Failed to erase NVS: %s", esp_err_to_name(ret));
               // Consider: erase all NVS? Or continue with defaults?
               return;  // Don't abort, just fail gracefully
           }
           ret = nvs_flash_init();
       }
       
       if (ret != ESP_OK) {
           LOG_E("NVS", "Failed to initialize NVS: %s", esp_err_to_name(ret));
           // Consider: system can continue without NVS?
           return;
       }
   }
   ```

2. **Fix USB CDC initialization**:
   ```cpp
   // In main.cpp
   if (!usb_cdc_init()) {
       LOG_W("Main", "USB CDC init failed - serial output may be limited");
       // System can continue, just no USB serial
   }
   ```

3. **Fix SPI initialization**:
   ```cpp
   // In epdspi.cpp
   esp_err_t ret = spi_bus_initialize(HSPI_PORT, &spi_default_cfg, 1);
   if (ret != ESP_OK) {
       LOG_E("EPD", "SPI init failed: %s", esp_err_to_name(ret));
       return false;  // Return error, don't abort
   }
   ```

4. **Remove `ESP_ERROR_CHECK` usage**:
   - Search for all `ESP_ERROR_CHECK` calls
   - Evaluate if each failure should abort or continue
   - Replace with proper error handling

5. **Add test cases**:
   - Mock NVS erase failure
   - Verify system doesn't abort
   - Mock SPI init failure
   - Verify proper error return

## References
- ESP-IDF Error Handling: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/error-handling.html
- ESP_ERROR_CHECK Macro: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/error-handling.html#simple-error-check-macro

</content>