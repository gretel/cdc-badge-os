---
title: "[MEDIUM] Sleep controller NVS load/save failures silently ignored"
severity: MEDIUM
domain: error-path-tests
lens: power-management
labels:
  - "sleep"
  - "nvs"
  - "power"
---

## Summary
In `components/cdc_hal/src/SleepController.cpp` (lines 369-389), the `loadFromNvs()` and `saveToNvs()` functions silently ignore NVS errors. This means sleep configuration may be lost on reboot or saved incorrectly without any indication.

**Files:**
- `components/cdc_hal/src/SleepController.cpp:369-389`

## Impact
1. **Lost Settings**: Sleep configuration lost on power cycle
2. **Battery Drain**: Default settings may be more aggressive than user wants
3. **Inconsistent State**: Settings appear saved but revert to defaults
4. **Debug Difficulty**: No error message when settings fail to save

## Evidence
From `components/cdc_hal/src/SleepController.cpp`:

```cpp
// Lines 369-379: loadFromNvs() - silent failure
bool SleepController::loadFromNvs() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sleep", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        // ❌ No error logging!
        return false;  // Returns false but no info why
    }

    // Load values
    uint32_t backlight = 30;  // Default
    nvs_get_u32(nvs, "backlight", &backlight);  // ❌ No error check!
    
    uint32_t lightSleep = 5;
    nvs_get_u32(nvs, "light_sleep", &lightSleep);  // ❌ No error check!
    
    uint32_t deepSleep = 30;
    nvs_get_u32(nvs, "deep_sleep", &deepSleep);  // ❌ No error check!
    
    nvs_close(nvs);
    return true;
}

// Lines 382-389: saveToNvs() - silent failure
bool SleepController::saveToNvs() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sleep", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        // ❌ No error logging!
        return false;
    }

    nvs_set_u32(nvs, "backlight", backlight);  // ❌ No error check!
    nvs_set_u32(nvs, "light_sleep", lightSleep);  // ❌ No error check!
    nvs_set_u32(nvs, "deep_sleep", deepSleep);  // ❌ No error check!
    
    nvs_commit(nvs);  // ❌ No error check!
    
    nvs_close(nvs);
    return true;
}
```

**Problem:**
- NVS open failures return false silently
- NVS get/set failures not checked
- NVS commit failures not checked
- No logging of what went wrong

## Recommended Fix
Add proper error logging and checking:

1. **Fix loadFromNvs()**:
   ```cpp
   bool SleepController::loadFromNvs() {
       nvs_handle_t nvs;
       esp_err_t err = nvs_open("sleep", NVS_READONLY, &nvs);
       if (err != ESP_OK) {
           LOG_W("Sleep", "Failed to open NVS: %s", esp_err_to_name(err));
           return false;
       }

       // Load values with error checking
       uint32_t backlight;
       err = nvs_get_u32(nvs, "backlight", &backlight);
       if (err == ESP_OK) {
           this->backlight = backlight;
       } else if (err != ESP_ERR_NVS_NOT_FOUND) {
           // Found but wrong type?
           LOG_W("Sleep", "Failed to read backlight: %s", esp_err_to_name(err));
       }
       // ESP_ERR_NVS_NOT_FOUND is OK - use default

       uint32_t lightSleep;
       err = nvs_get_u32(nvs, "light_sleep", &lightSleep);
       if (err == ESP_OK) {
           this->lightSleep = lightSleep;
       } else if (err != ESP_ERR_NVS_NOT_FOUND) {
           LOG_W("Sleep", "Failed to read light_sleep: %s", esp_err_to_name(err));
       }

       uint32_t deepSleep;
       err = nvs_get_u32(nvs, "deep_sleep", &deepSleep);
       if (err == ESP_OK) {
           this->deepSleep = deepSleep;
       } else if (err != ESP_ERR_NVS_NOT_FOUND) {
           LOG_W("Sleep", "Failed to read deep_sleep: %s", esp_err_to_name(err));
       }

       nvs_close(nvs);
       return true;  // Always return true - defaults are OK
   }
   ```

2. **Fix saveToNvs()**:
   ```cpp
   bool SleepController::saveToNvs() {
       nvs_handle_t nvs;
       esp_err_t err = nvs_open("sleep", NVS_READWRITE, &nvs);
       if (err != ESP_OK) {
           LOG_E("Sleep", "Failed to open NVS: %s", esp_err_to_name(err));
           return false;
       }

       err = nvs_set_u32(nvs, "backlight", backlight);
       if (err != ESP_OK) {
           LOG_E("Sleep", "Failed to set backlight: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return false;
       }

       err = nvs_set_u32(nvs, "light_sleep", lightSleep);
       if (err != ESP_OK) {
           LOG_E("Sleep", "Failed to set light_sleep: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return false;
       }

       err = nvs_set_u32(nvs, "deep_sleep", deepSleep);
       if (err != ESP_OK) {
           LOG_E("Sleep", "Failed to set deep_sleep: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return false;
       }

       err = nvs_commit(nvs);
       if (err != ESP_OK) {
           LOG_E("Sleep", "Failed to commit: %s", esp_err_to_name(err));
           nvs_close(nvs);
           return false;
       }

       nvs_close(nvs);
       return true;
   }
   ```

3. **Add test cases**:
   - Mock NVS to simulate open failure
   - Verify error is logged
   - Mock NVS to simulate commit failure
   - Verify error is returned and logged
   - Test with missing keys (default values should be used)

## References
- ESP-IDF NVS Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- NVS Error Codes: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html#error-codes

</content>