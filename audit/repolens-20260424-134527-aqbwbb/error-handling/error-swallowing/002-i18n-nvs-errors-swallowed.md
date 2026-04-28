---
title: "[MEDIUM] I18n NVS load/save functions silently swallow errors"
severity: MEDIUM
domain: cdc_ui
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/cdc_ui/src/I18n.cpp:169-195`, the `loadFromNvs()` and `saveToNvs()` functions silently ignore NVS errors. When NVS operations fail, the functions just do nothing without logging or returning status, making it impossible to detect configuration persistence issues.

**Location:** `components/cdc_ui/src/I18n.cpp:169-195`

```cpp
void I18n::loadFromNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint8_t lang = 0;
        if (nvs_get_u8(handle, NVS_KEY_LANG, &lang) == ESP_OK) {
            if (lang < static_cast<uint8_t>(Language::COUNT)) {
                currentLang_ = static_cast<Language>(lang);
            }
        }
        nvs_close(handle);
    }
    // Silent failure - no log, no return value
}

void I18n::saveToNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_LANG, static_cast<uint8_t>(currentLang_));
        nvs_commit(handle);
        nvs_close(handle);
    }
    // Silent failure - no log, no return value
}
```

## Impact
- Language preference may fail to persist without any indication
- On first boot or after NVS corruption, language silently resets to default
- Hard to debug why language settings aren't being saved
- NVS full/corruption errors go undetected

## Evidence
Both functions:
1. Open NVS but only check for success (`if (err == ESP_OK)`)
2. Do nothing when NVS open fails (no log, no error status)
3. Don't check `nvs_commit()` result in `saveToNvs()`
4. Return `void` so callers cannot detect failures

## Recommended Fix
Add logging and return status:

```cpp
/**
 * \brief Loads selected language from NVS.
 * \return `true` on success, `false` if NVS load failed.
 */
bool I18n::loadFromNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_D(TAG, "NVS open failed for language load: %s", esp_err_to_name(err));
        return false;
    }
    uint8_t lang = 0;
    if (nvs_get_u8(handle, NVS_KEY_LANG, &lang) == ESP_OK) {
        if (lang < static_cast<uint8_t>(Language::COUNT)) {
            currentLang_ = static_cast<Language>(lang);
        }
    }
    nvs_close(handle);
    return true;
}

/**
 * \brief Saves selected language to NVS.
 * \return `true` on success, `false` if NVS save failed.
 */
bool I18n::saveToNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_W(TAG, "NVS open failed for language save: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_u8(handle, NVS_KEY_LANG, static_cast<uint8_t>(currentLang_));
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
- Error handling in embedded systems: https://www.embedded.com/electronics-blogs/embedded-essentials/4026449/Embedded-error-handling-part-1.aspx
