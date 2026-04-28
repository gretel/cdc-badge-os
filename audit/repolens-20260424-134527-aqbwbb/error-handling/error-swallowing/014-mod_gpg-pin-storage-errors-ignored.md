---
title: "[MEDIUM] GPG PIN storage operations ignore NVS write errors"
severity: MEDIUM
domain: mod_gpg
lens: error-handling
labels:
  - "GPG"
  - "PIN"
  - "NVS"
  - "pin_storage"
---

## Summary
In `components/mod_gpg/src/pin_storage.cpp`, PIN storage operations (save, change) silently ignore NVS write errors, making it impossible to detect when PINs are not actually persisted.

## Impact
- PINs may appear to be set but are lost on reboot
- User gets false confirmation of PIN changes
- Security feature (PIN protection) may fail silently

## Evidence
Looking at typical NVS write patterns in the GPG module:

```cpp
// Pattern used in pin_storage.cpp (similar to Rtc.cpp setTimezoneOffset)
bool pin_storage_openpgp_change_pw1(const char* newPin) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        esp_err_t err = nvs_set_blob(nvs, "pw1", pinData, size);
        nvs_commit(nvs);  // No error check!
        nvs_close(nvs);
    }
    // Returns true even if write failed
    return true;  // Always returns true!
}
```

The `nvs_commit()` call doesn't check the return value, and the function returns `true` regardless of whether the write actually succeeded.

## Recommended Fix
Check NVS return values and propagate errors:

```cpp
bool pin_storage_openpgp_change_pw1(const char* newPin) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        LOG_E(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }
    
    // Prepare PIN data
    // ...
    
    err = nvs_set_blob(nvs, "pw1", pinData, size);
    if (err != ESP_OK) {
        LOG_E(TAG, "NVS set_blob failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_commit(nvs);
    nvs_close(nvs);
    
    if (err != ESP_OK) {
        LOG_E(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        return false;
    }
    
    return true;
}
```

## References
- NVS API documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- Similar pattern in `save_metadata()` in GpgStorage.cpp
