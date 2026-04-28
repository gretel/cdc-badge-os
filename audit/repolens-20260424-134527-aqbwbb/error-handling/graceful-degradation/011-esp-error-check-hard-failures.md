---
title: "[MEDIUM] ESP_ERROR_CHECK Causes Hard System Reset Instead of Graceful Recovery"
severity: MEDIUM
domain: system-initialization
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `main/main.cpp:59-62`, `ESP_ERROR_CHECK` is used for NVS initialization. When `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, the code erases NVS and re-initializes. However, if `nvs_flash_erase()` or the second `nvs_flash_init()` fails, `ESP_ERROR_CHECK` causes an immediate system reset (abort), with no opportunity for graceful degradation or user notification.

Additionally, `ESP_ERROR_CHECK` is used in `BluetoothController.cpp:465` for `esp_bt_controller_mem_release()`, which can fail on some ESP32-S3 configurations, causing an unnecessary hard reset.

Lines of interest:
- `main/main.cpp:58-62` - NVS initialization with `ESP_ERROR_CHECK`
- `components/cdc_hal/src/BluetoothController.cpp:465` - BT memory release with `ESP_ERROR_CHECK`

## Impact
When NVS initialization fails:
1. System resets immediately without logging the actual error
2. User sees no indication of what went wrong
3. No fallback to safe defaults or read-only mode
4. In worst case (corrupted NVS), continuous reboot loop

When Bluetooth memory release fails:
1. System resets even though BLE-only operation could continue
2. Classic BT memory release is optional for BLE-only designs

## Evidence
```cpp
// main/main.cpp:58-62
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());  // Hard fail if erase fails!
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);  // Hard fail if re-init fails!
```

```cpp
// BluetoothController.cpp:465
ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
```

## Recommended Fix
Replace `ESP_ERROR_CHECK` with explicit error handling that allows graceful degradation:

```cpp
// For NVS initialization
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // Attempt erase, but continue with warning if it fails
    esp_err_t eraseRet = nvs_flash_erase();
    if (eraseRet != ESP_OK) {
        LOG_W(TAG, "NVS erase failed (%s), continuing with limited functionality", esp_err_to_name(eraseRet));
        // Continue - some modules may work without NVS
    } else {
        ret = nvs_flash_init();
    }
}
if (ret != ESP_OK) {
    LOG_W(TAG, "NVS init failed (%s), operating in read-only mode", esp_err_to_name(ret));
    // Set flag to disable NVS-dependent features, continue boot
}
```

```cpp
// For Bluetooth memory release
esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
if (ret != ESP_OK) {
    LOG_W(TAG, "BT memory release failed (%s), continuing with full stack", esp_err_to_name(ret));
    // Fine to continue - just uses slightly more memory
}
```

## References
- [ESP-IDF Error Handling Best Practices](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/error-handling.html)
- [Graceful Degradation Pattern](https://en.wikipedia.org/wiki/Graceful_degradation)
- ESP_ERROR_CHECK expands to `assert`-like behavior that calls `abort()` on non-ESP_OK results
