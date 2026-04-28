---
title: "[MEDIUM] NVS_CLEAR command lacks rollback verification"
severity: MEDIUM
domain: database
lens: query-safety
labels:
  - audit:database/query-safety
---

## Summary
The `NVS_CLEAR` command (line 493-516 in `components/serial_cmd/src/SerialCmd.cpp`) performs a full NVS erase with `nvs_flash_erase()` but does not verify the result or provide rollback capability. Once executed, all NVS data is irrecoverably lost.

**Evidence:**
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Lines: 504-515
```cpp
Console::printf("Clearing NVS...\r\n");
esp_err_t err = nvs_flash_erase();
if (err != ESP_OK) {
    Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
    return;
}
err = nvs_flash_init();
if (err != ESP_OK) {
    Console::printf("ERROR: NVS init failed (%s)\r\n", esp_err_to_name(err));
    return;
}
Console::printf("OK: NVS cleared. Reboot recommended.\r\n");
```

## Impact
- **Complete Data Loss**: Erases all NVS including module settings, PINs, WiFi credentials, TOTP accounts, password vault
- **No Verification**: Does not verify erase completed successfully before reporting "OK"
- **No Rollback**: If `nvs_flash_init()` fails, the NVS is left in an inconsistent state
- **Potential Brick**: Device may become unusable if NVS initialization fails after erase

## Recommended Fix
1. Add pre-erase verification (e.g., check NVS is readable before wiping)
2. Add post-erase verification (read back a known key to confirm erase)
3. Consider adding a "dry-run" mode that shows what would be erased
4. Log the erase operation to a persistent error log for audit trail

```cpp
// Before erase: verify NVS is in good state
err = nvs_flash_init();
if (err != ESP_OK) {
    Console::printf("WARNING: NVS not initialized, attempting recovery...\r\n");
}

// After erase: verify success
err = nvs_flash_erase();
if (err != ESP_OK) {
    Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
    return;
}

// Verify erase completed
nvs_handle_t nvs;
err = nvs_open("verification", NVS_READONLY, &nvs);
if (err == ESP_OK) {
    // Should be empty/defaults
    nvs_close(nvs);
}
```

## References
- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- Similar pattern in codebase: `cmdTr01Wipe` includes progress reporting
