---
title: "[MEDIUM] GPG cardholder name, URL, and login data logged to serial console"
severity: MEDIUM
domain: mod_gpg
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The GPG module in `components/mod_gpg/src/openpgp/openpgp.cpp` logs cardholder metadata (name, URL, login) to the serial console via `ESP_LOGI` when these fields are set. This data is output using the ESP_LOG macro which bypasses the cdc_log library and may appear on multiple output channels including UART and USB CDC.

## Impact
- **PII Exposure**: Cardholder names (e.g., "John Doe"), URLs (public key locations), and login data (email addresses, usernames) are transmitted in plaintext over serial
- **Multiple Transport Paths**: `ESP_LOG` writes directly to UART and may also appear on USB CDC, creating multiple exposure vectors
- **Persistent Logs**: Serial output is commonly captured to log files, terminal buffers, or forwarded to remote logging systems
- **User Enumeration**: The logged data reveals user identities stored on the device

## Evidence
File: `components/mod_gpg/src/openpgp/openpgp.cpp`

Lines 911-912 (Cardholder name):
```cpp
ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);
```

Lines 1017-1018 (URL):
```cpp
ESP_LOGI(TAG, "URL set: %s", cardholder_url);
```

Lines 1028-1029 (Login):
```cpp
ESP_LOGI(TAG, "Login set: %s", cardholder_login);
```

These are called during `PUT DATA` APDU processing when cardholder metadata is updated via the OpenPGP card interface.

## Recommended Fix
1. **Use cdc_log instead of ESP_LOG**: Replace `ESP_LOGI` with `LOG_I` from cdc_log for consistent output routing
2. **Mask sensitive fields**: Show only partial data or a placeholder
3. **Add debug-only flag**: Only log full details when `DEBUG_MODE` is enabled

Example fix:
```cpp
// Before:
ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);

// After:
#if DEBUG_MODE
    LOG_I(TAG, "Cardholder name set: %s", cardholder_name);
#else
    LOG_I(TAG, "Cardholder name set: %.1s...", cardholder_name);  // Show first char only
#endif
```

## References
- ESP32 logging: [ESP_LOG macros](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html)
- cdc_log library: `components/cdc_log/include/cdc_log.h`
- Related finding: #010 (ESP_LOG bypasses cdc_log)
- Related finding: #007 (GPG PIN status logged)

</content>