---
title: "[MEDIUM] Esp32SleepController NVS load/save functions silently swallow errors"
severity: MEDIUM
domain: cdc_hal
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/cdc_hal/src/SleepController.cpp:367-390`, the `Esp32SleepController::loadFromNvs()` and `saveToNvs()` functions silently ignore NVS errors. When NVS operations fail, the functions just do nothing without logging or returning status.

**Location:** `components/cdc_hal/src/SleepController.cpp:367-390`

```cpp
void Esp32SleepController::loadFromNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint32_t interval = 0;
        if (nvs_get_u32(handle, NVS_KEY_INTERVAL, &interval) == ESP_OK) {
            lightSleepIntervalS_ = interval;
        }
        nvs_close(handle);
    }
    // Silent failure - no log, no return value
}

void Esp32SleepController::saveToNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u32(handle, NVS_KEY_INTERVAL, lightSleepIntervalS_);
        nvs_commit(handle);
        nvs_close(handle);
    }
    // Silent failure - no log, no return value
}
```

## Impact
- Sleep interval preference may fail to persist without any indication
- Sleep configuration may silently reset to defaults on reboot
- Hard to debug why sleep settings aren't being saved
- NVS full/corruption errors go undetected
- Power management behavior may be inconsistent

## Evidence
Both functions:
1. Open NVS but only check for success (`if (err == ESP_OK)`)
2. Do nothing when NVS open fails (no log, no error status)
3. Don't check `nvs_commit()` result in `saveToNvs()`
4. Return `void` so callers cannot detect failures
5. `loadFromNvs()` doesn't log when value not found

## Recommended Fix
Add logging and return status:

```cpp
/**
 * \brief Loads persisted light-sleep interval from NVS.
 * \return `true` on success, `false` if NVS load failed.
 */
bool Esp32SleepController::loadFromNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_D(TAG, "NVS open failed for sleep interval load: %s", esp_err_to_name(err));
        return false;
    }
    uint32_t interval = 0;
    err = nvs_get_u32(handle, NVS_KEY_INTERVAL, &interval);
    nvs_close(handle);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            LOG_W(TAG, "Failed to read sleep interval: %s", esp_err_to_name(err));
        }
        return false;
    }
    lightSleepIntervalS_ = interval;
    return true;
}

/**
 * \brief Persists current light-sleep interval to NVS.
 * \return `true` on success, `false` if NVS save failed.
 */
bool Esp32SleepController::saveToNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_W(TAG, "NVS open failed for sleep interval save: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_u32(handle, NVS_KEY_INTERVAL, lightSleepIntervalS_);
    if (err != ESP_OK) {
        LOG_W(TAG, "NVS set failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        LOG_W(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
```

## References
- ESP-IDF NVS Documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
