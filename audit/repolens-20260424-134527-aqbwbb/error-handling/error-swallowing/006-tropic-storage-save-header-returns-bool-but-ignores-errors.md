---
title: "[LOW] TropicStorage::saveHeader() doesn't log NVS errors on failure"
severity: LOW
domain: cdc_core
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp:350-361`, the `saveHeader()` function returns a boolean status but doesn't log why the save failed. Callers can detect failure but have no visibility into the cause.

**Location:** `components/cdc_core/src/TropicStorage.cpp:350-361`

```cpp
bool TropicStorage::saveHeader() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;  // No log!
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY_HEADER, &header_, sizeof(header_));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err == ESP_OK;  // No log on failure!
}
```

## Impact
- Cache persistence failures are silent
- Hard to debug why cache isn't being saved
- NVS full/corruption issues go undetected
- May cause unexpected cache reloads on reboot

## Evidence
The function returns `false` when:
1. `nvs_open()` fails (no log)
2. `nvs_set_blob()` fails (no log)
3. `nvs_commit()` fails (no log)

Compare with `TropicStorage::loadHeader()` at line 322 which also doesn't log failures.

## Recommended Fix
Add debug logging for failures:

```cpp
bool TropicStorage::saveHeader() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        LOG_D(TAG, "NVS open failed for header save: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_blob(nvs, NVS_KEY_HEADER, &header_, sizeof(header_));
    if (err != ESP_OK) {
        LOG_W(TAG, "NVS set failed for header: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return false;
    }
    err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) {
        LOG_W(TAG, "NVS commit failed for header: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
```

## References
- ESP-IDF NVS Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
