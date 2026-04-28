---
title: "[MEDIUM] WiFi password stored in NVS without encryption"
severity: MEDIUM
domain: secrets
lens: secrets-credential-management
labels:
  - "audit:security/secrets"
---

## Summary
The WiFi password is stored in NVS (Non-Volatile Storage) using `nvs_set_str()` without any encryption. This is in `components/cdc_os_ui/src/WifiHandlers.cpp:86` (load) and `components/cdc_os_ui/src/WifiHandlers.cpp:114` (save).

## Impact
**Security Risk**: NVS storage on ESP32 is not encrypted by default. Anyone with physical access to the device can:
- Dump NVS partitions using tools like `esptool.py`
- Extract the WiFi password in plaintext
- Access the user's WiFi network

While better than hardcoded credentials, this is still a moderate risk for a security-focused device.

## Evidence

**File: `components/cdc_os_ui/src/WifiHandlers.cpp`**
```cpp
// Lines 76-90 - Loading WiFi config
void WifiHandlers::loadConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {
        config_.valid = false;
        return;
    }

    size_t len = sizeof(config_.ssid);
    if (nvs_get_str(nvs, "ssid", config_.ssid, &len) != ESP_OK || len <= 1) {
        nvs_close(nvs);
        config_.valid = false;
        return;
    }

    len = sizeof(config_.password);
    nvs_get_str(nvs, "pass", config_.password, &len);  // <-- Plaintext read
    nvs_get_u8(nvs, "sec", &config_.security);
    ...
}

// Lines 107-125 - Saving WiFi config
void WifiHandlers::saveConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK) return;

    nvs_set_str(nvs, "ssid", wizard_.ssid);
    nvs_set_str(nvs, "pass", wizard_.password);  // <-- Plaintext write
    nvs_set_u8(nvs, "sec", static_cast<uint8_t>(wizard_.security));
    ...
}
```

## Recommended Fix
1. **Use ESP32 flash encryption**: Enable ESP-IDF flash encryption feature to encrypt all NVS data at rest.

2. **Use NVS encryption**: Enable ESP32's NVS encryption feature:
   ```cpp
   // In sdkconfig.defaults
   CONFIG_NVS_ENCRYPTION=y
   CONFIG_NVS_FLASH_ENCRYPTION=y
   ```

3. **Encrypt before storing**: Use AES-256 to encrypt the password before storing:
   ```cpp
   // Generate key from device-unique ID or use secure element
   uint8_t key[32];
   // Derive key from TROPIC01 or efuse
   nvs_set_blob(nvs, "pass", encryptedPassword, encryptedLen);
   ```

4. **Use secure element**: Store the WiFi password in TROPIC01 R-Memory instead of NVS.

## References
- [ESP32 NVS Encryption Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#encryption)
- [ESP32 Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html)
- [OWASP Secrets Management](https://cheatsheetseries.owasp.org/cheatsheets/Secrets_Management_Cheat_Sheet.html)
