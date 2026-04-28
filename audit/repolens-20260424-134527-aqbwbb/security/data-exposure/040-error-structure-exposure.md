---
title: "[LOW] esp_err_to_name exposes ESP32 internal error codes and structure"
severity: LOW
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
Multiple serial commands use `esp_err_to_name()` to display ESP32 error codes in human-readable format. This exposes internal ESP-IDF error code structure and can help attackers fingerprint the exact ESP-IDF version.

**Locations:**
- `components/serial_cmd/src/SerialCmd.cpp:507-649` - NVS commands
- `components/mod_nvsedit/src/NvsEditModule.cpp:166-187` - NVS delete commands
- `components/usb_badge/usb_cdc.cpp:61-121` - USB initialization
- `components/cdc_hal/src/WifiController.cpp:199-625` - WiFi commands

## Impact
- **Version fingerprinting**: Error code names reveal exact ESP-IDF version
- **Internal structure**: Error code values expose internal ESP32 structure
- **Attack surface mapping**: Helps attackers understand which subsystems are used
- **Debug info leakage**: Error details may reveal implementation choices

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp:507`
```cpp
Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
```

File: `components/serial_cmd/src/SerialCmd.cpp:590`
```cpp
Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
```

File: `components/cdc_hal/src/WifiController.cpp:199`
```cpp
LOG_E(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
```

Common error codes exposed:
- `ESP_OK`, `ESP_FAIL`
- `ESP_ERR_NO_MEM`, `ESP_ERR_INVALID_ARG`
- `ESP_ERR_NVS_NOT_FOUND`, `ESP_ERR_WIFI_NOT_INIT`
- And many more ESP-IDF specific codes

## Recommended Fix
1. Replace esp_err_to_name with generic error messages in production
2. Use a mapping function to translate to user-friendly messages
3. Only show error codes, not names, for less fingerprinting

Example fix:
```cpp
// Instead of:
Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));

// Use:
const char* errToString(esp_err_t err) {
    switch (err) {
        case ESP_OK: return "Success";
        case ESP_ERR_NO_MEM: return "Out of memory";
        case ESP_ERR_INVALID_ARG: return "Invalid argument";
        default: return "Error";
    }
}
Console::printf("ERROR: NVS erase failed (%s)\r\n", errToString(err));
```

## References
- ESP-IDF error codes: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/error_codes.html
- Error code fingerprinting: https://en.wikipedia.org/wiki/Fingerprinting_(computing)
- Related to issue #19 (verbose error messages)
