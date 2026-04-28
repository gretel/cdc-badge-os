---
title: "[LOW] Esp32Rtc::loadTimezoneFromNvs() silently ignores NVS read errors"
severity: LOW
domain: cdc_hal
lens: error-handling
labels:
  - "RTC"
  - "NVS"
  - "timezone"
---

## Summary
In `components/cdc_hal/src/Rtc.cpp`, the `loadTimezoneFromNvs()` method (lines 252-262) silently ignores NVS read errors, making it impossible to distinguish between "no timezone set" and "NVS read failure".

## Impact
- NVS corruption or wear failures are hidden
- Timezone may be incorrectly set to 0 (UTC) without warning
- Difficult to diagnose NVS issues affecting time display

## Evidence
```cpp
// Lines 252-262 in Rtc.cpp
void Esp32Rtc::loadTimezoneFromNvs() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        int8_t tz = 0;
        if (nvs_get_i8(nvs, NVS_KEY_TZ, &tz) == ESP_OK) {
            tzOffset_ = tz;
        }
        nvs_close(nvs);
    }
    // All errors silently ignored - timezone stays at default 0
}
```

Compare to `setTimezoneOffset()` (lines 228-248) which writes but doesn't verify:
```cpp
void Esp32Rtc::setTimezoneOffset(int8_t hours) {
    // ... validation ...
    // Save to NVS
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_i8(nvs, NVS_KEY_TZ, tzOffset_);
        nvs_commit(nvs);  // No error check!
        nvs_close(nvs);
    }
    // Write failures silently ignored
}
```

## Recommended Fix
Add debug logging for NVS errors to aid diagnosis:

```cpp
void Esp32Rtc::loadTimezoneFromNvs() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        LOG_D(TAG, "NVS open failed (err=%s)", esp_err_to_name(err));
        return;
    }
    
    int8_t tz = 0;
    err = nvs_get_i8(nvs, NVS_KEY_TZ, &tz);
    if (err == ESP_OK) {
        tzOffset_ = tz;
        LOG_D(TAG, "Loaded timezone: UTC%+d", tz);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        LOG_D(TAG, "Timezone not in NVS, using default UTC0");
    } else {
        LOG_W(TAG, "NVS get_i8 failed (err=%s)", esp_err_to_name(err));
    }
    
    nvs_close(nvs);
}
```

## References
- ESP32 NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- Similar NVS usage in `save_metadata()` in GPG module
