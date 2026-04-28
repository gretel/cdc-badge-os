---
title: "[MEDIUM] Multiple modules silently ignore NVS commit failures"
severity: MEDIUM
domain: multiple
lens: error-handling
labels:
  - "NVS"
  - "persistence"
  - "settings"
---

## Summary

Multiple modules in the codebase call `nvs_commit()` without checking the return value, silently swallowing commit failures. This affects:

1. **Grove LED module** - `components/grove_led/src/GroveLedModule.cpp:625`
2. **BLE HID keyboard** - `components/mod_hid/src/BleHidKeyboard.cpp:528`
3. **BLE Serial module** - `components/mod_ble_serial/src/BleSerialModule.cpp:183`

In each case, `nvs_commit()` is called without checking if it returns `ESP_OK`, meaning settings may appear to be saved but are actually lost on reboot.

## Impact

- User settings (LED configuration, keyboard unicode method, BLE serial enabled state) may appear to be saved but are lost on reboot
- No error indication to the user that persistence failed
- NVS wear or corruption issues are hidden from operators
- Debugging becomes difficult as the symptom (settings not persisting) doesn't match the cause (commit failure)

## Evidence

### Grove LED Module (line 615-627)
```cpp
void GroveLedModule::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_ENABLED, enabled_ ? 1 : 0);
        nvs_set_u8(handle, NVS_KEY_LED_COUNT, ledCount_);
        nvs_set_u8(handle, NVS_KEY_BRIGHTNESS, brightness_);
        nvs_set_u8(handle, NVS_KEY_COLOR_R, staticR_);
        nvs_set_u8(handle, NVS_KEY_COLOR_G, staticG_);
        nvs_set_u8(handle, NVS_KEY_COLOR_B, staticB_);
        nvs_set_u8(handle, NVS_KEY_EFFECT, static_cast<uint8_t>(effect_));
        nvs_commit(handle);  // <-- Return value ignored!
        nvs_close(handle);
    }
}
```

### BLE HID Keyboard (line 524-531)
```cpp
void BleHidKeyboard::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_UNICODE, static_cast<uint8_t>(unicodeMethod_));
        nvs_commit(handle);  // <-- Return value ignored!
        nvs_close(handle);
    }
}
```

### BLE Serial Module (line 179-186)
```cpp
void BleSerialModule::saveSettings() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "enabled", enabled_ ? 1 : 0);
        nvs_commit(nvs);  // <-- Return value ignored!
        nvs_close(nvs);
    }
}
```

## Recommended Fix

For each module, check the `nvs_commit()` return value and log an error if it fails:

```cpp
void GroveLedModule::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_ENABLED, enabled_ ? 1 : 0);
        // ... other sets ...
        esp_err_t err = nvs_commit(handle);
        if (err != ESP_OK) {
            LOG_E(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        }
        nvs_close(handle);
    }
}
```

Apply the same pattern to:
- `GroveLedModule::saveSettings()` in `components/grove_led/src/GroveLedModule.cpp`
- `BleHidKeyboard::saveSettings()` in `components/mod_hid/src/BleHidKeyboard.cpp`
- `BleSerialModule::saveSettings()` in `components/mod_ble_serial/src/BleSerialModule.cpp`

## References

- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/reference/storage/nvs_flash.html)
- Related findings:
  - `002-i18n-nvs-errors-swallowed.md` - I18n NVS errors
  - `003-sleep-controller-nvs-errors.md` - Sleep controller NVS errors
  - `007-fido2-counter-save-commits-error-swallowed.md` - FIDO2 NVS commit error
