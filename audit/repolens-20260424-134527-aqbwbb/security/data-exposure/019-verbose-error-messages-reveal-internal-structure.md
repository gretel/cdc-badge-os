---
title: "[MEDIUM] Verbose error messages with esp_err_to_name reveal internal ESP32 structure"
severity: MEDIUM
domain: serial-cmd, logging
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
Multiple error messages throughout the codebase use `esp_err_to_name(err)` to display detailed ESP32 error codes in serial output. This reveals internal ESP-IDF error names and structure, which can aid attackers in understanding the system internals and crafting targeted exploits.

## Impact
- **Internal Structure Exposure**: Error names like `ESP_ERR_NVS_NOT_FOUND`, `ESP_ERR_INVALID_ARG`, etc. reveal the underlying ESP-IDF framework and its error handling
- **Framework Versioning**: Specific error names can help identify the ESP-IDF version used
- **Attack Surface Mapping**: Error messages reveal which operations are performed (NVS, I2C, UART, etc.) and their failure points
- **Debug Information Leakage**: Error codes provide detailed diagnostic information useful for reconnaissance

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`

Multiple locations with verbose error messages:
```cpp
Line 507: Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
Line 512: Console::printf("ERROR: NVS init failed (%s)\r\n", esp_err_to_name(err));
Line 538: Console::printf("ERROR: nvs_entry_find failed (%s)\r\n", esp_err_to_name(err));
Line 590: Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
Line 629: Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
Line 639: Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
Line 649: Console::printf("ERROR: Delete failed (%s)\r\n", esp_err_to_name(err));
```

File: `components/mod_nvsedit/src/NvsEditModule.cpp`
```cpp
Line 166: LOG_I(TAG, "Deleted key '%s' from '%s': %s", key, ns, esp_err_to_name(err));
Line 187: LOG_I(TAG, "Deleted namespace '%s': %s", ns, esp_err_to_name(err));
```

File: `components/usb_badge/usb_cdc.cpp`
```cpp
Line 61:  LOG_E(TAG, "USB PHY init failed: %s", esp_err_to_name(err));
Line 121: LOG_E(TAG, "USB PHY init failed");
```

File: `components/cdc_hal/src/TCA9535Keypad.cpp`
```cpp
Line 232: LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
```

File: `components/cdc_views/src/QRCodeView.cpp`
```cpp
Line 143: LOG_E(TAG, "QR sizing failed: %s", esp_err_to_name(err));
Line 165: LOG_E(TAG, "QR render failed: %s", esp_err_to_name(err));
```

File: `components/grove_led/src/GroveLedModule.cpp`
```cpp
Line 138: LOG_E(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
```

Total: 31 occurrences across the codebase.

## Recommended Fix
1. **Replace esp_err_to_name with generic messages**: Use simple error codes or generic descriptions
2. **Add debug-only flag**: Only show detailed errors when DEBUG_MODE is enabled
3. **Use error codes only**: Log numeric error codes instead of names for debugging

Example fix:
```cpp
// Before:
Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));

// After (production):
Console::printf("ERROR: NVS erase failed (code: %d)\r\n", err);

// After (DEBUG_MODE only):
#if DEBUG_MODE
Console::printf("ERROR: NVS erase failed (%s, code: %d)\r\n", esp_err_to_name(err), err);
#endif
```

## References
- OWASP: [Error Handling](https://owasp.org/www-project-cheat-sheets/cheatsheets/Error_Handling_Cheat_Sheet.html)
- ESP-IDF documentation: [Error Codes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/err.html)
- Related finding: #010 (ESP_LOG bypasses cdc_log)

</content>