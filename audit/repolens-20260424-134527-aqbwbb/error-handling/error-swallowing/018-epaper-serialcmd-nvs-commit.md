---
title: "[LOW] EpaperDisplay and SerialCmd NVS commit errors not checked"
severity: LOW
domain: cdc_hal, serial_cmd
lens: error-handling
labels:
  - "NVS"
  - "persistence"
  - "EpaperDisplay"
  - "SerialCmd"
---

## Summary

Two locations in the codebase call `nvs_commit()` without checking the return value:

1. **EpaperDisplay** - `components/cdc_hal/src/EpaperDisplay.cpp:95` - `persistBacklight()` helper function
2. **SerialCmd** - `components/serial_cmd/src/SerialCmd.cpp:636,644` - NVS erase/delete commands

While these are less critical (backlight persistence, admin NVS commands), the pattern of ignoring commit errors should be corrected for consistency.

## Impact

### EpaperDisplay
- Backlight preference may not persist across reboots without user noticing
- No indication that NVS write failed

### SerialCmd
- NVS erase commands report "OK" even if commit fails
- Admin debugging may be confused by apparent success

## Evidence

### EpaperDisplay (line 91-99)
```cpp
static void persistBacklight(uint16_t level) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u16(nvs, NVS_KEY_BACKLIGHT, level);
        nvs_commit(nvs);  // <-- Return value ignored!
        nvs_close(nvs);
        LOG_D(TAG, "Backlight saved to NVS: %u", level);
    }
}
```

### SerialCmd (line 633-651)
```cpp
if (parsed == 1 || key[0] == '\0') {
    err = nvs_erase_all(nvs);
    if (err == ESP_OK) {
        nvs_commit(nvs);  // <-- Return value ignored!
        Console::printf("OK: Namespace '%s' erased\r\n", ns);
    } else {
        Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
    }
} else {
    err = nvs_erase_key(nvs, key);
    if (err == ESP_OK) {
        nvs_commit(nvs);  // <-- Return value ignored!
        Console::printf("OK: Key '%s.%s' deleted\r\n", ns, key);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        Console::printf("ERROR: Key '%s' not found\r\n", key);
    } else {
        Console::printf("ERROR: Delete failed (%s)\r\n", esp_err_to_name(err));
    }
}
```

Note: The erase operation is checked, but the commit that follows is not.

## Recommended Fix

### EpaperDisplay
```cpp
static void persistBacklight(uint16_t level) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u16(nvs, NVS_KEY_BACKLIGHT, level);
        esp_err_t err = nvs_commit(nvs);
        if (err != ESP_OK) {
            LOG_W(TAG, "Backlight NVS commit failed: %s", esp_err_to_name(err));
        }
        nvs_close(nvs);
        LOG_D(TAG, "Backlight saved to NVS: %u", level);
    }
}
```

### SerialCmd
```cpp
if (parsed == 1 || key[0] == '\0') {
    err = nvs_erase_all(nvs);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
        if (err == ESP_OK) {
            Console::printf("OK: Namespace '%s' erased\r\n", ns);
        } else {
            Console::printf("ERROR: Commit failed (%s)\r\n", esp_err_to_name(err));
        }
    } else {
        Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
    }
}
// Similar fix for the erase_key branch
```

## References

- Related findings:
  - `017-multiple-modules-nvs-commit-errors.md` - Other modules with same issue
  - `002-i18n-nvs-errors-swallowed.md` - I18n NVS errors
  - `003-sleep-controller-nvs-errors.md` - Sleep controller NVS errors
