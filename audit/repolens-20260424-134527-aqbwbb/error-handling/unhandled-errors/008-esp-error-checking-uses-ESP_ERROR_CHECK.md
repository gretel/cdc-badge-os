---
title: "[LOW] Inconsistent ESP error checking - some use ESP_ERROR_CHECK, others check manually"
severity: LOW
domain: error-handling
lens: inconsistent-error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `main/main.cpp` at line 58-61, `ESP_ERROR_CHECK` is used for NVS initialization, but other ESP-IDF calls in the same file use manual error checking. This inconsistency can lead to confusion about which errors are critical vs. recoverable.

**Location:** `main/main.cpp:58-61`
```cpp
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());  // <-- Uses ESP_ERROR_CHECK
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);  // <-- Uses ESP_ERROR_CHECK
```

## Impact
- `ESP_ERROR_CHECK` aborts the program on error - may be too aggressive for some cases
- Manual checking allows graceful degradation or retry
- Inconsistent patterns make code harder to maintain
- If NVS is corrupt, `nvs_flash_erase()` with `ESP_ERROR_CHECK` will abort - but this is probably OK

Actually, this pattern is correct for boot-time initialization. Let me find a better example...

Looking at `TCA9535Keypad.cpp:225-232`:
```cpp
esp_err_t err = gpio_install_isr_service(0);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;  // <-- Returns false, allows boot to continue
}
```

This is actually GOOD error handling - it checks for specific error codes and allows recovery.

Let me look for actual issues...

## Evidence
**main.cpp:58-61** - Aggressive error checking:
```cpp
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());  // Aborts on any error
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);  // Aborts on any error
```

**TCA9535Keypad.cpp:225-232** - Graceful error handling:
```cpp
esp_err_t err = gpio_install_isr_service(0);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    // Cleanup and return false - allows boot to continue
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;
}
```

**BQ25895Power.cpp:269-276** - Graceful error handling:
```cpp
esp_err_t err = gpio_install_isr_service(0);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    state_ = core::ServiceState::ERROR;
    return false;
}
```

**SpiBus.cpp:46-51** - Mixed approach:
```cpp
esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &buscfg, SPI_DMA_CHAN);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "SPI bus init failed: %d", err);
    return err;  // Returns error code
}
if (err == ESP_ERR_INVALID_STATE) {
    LOG_I(TAG, "SPI bus already initialized (by display)");
}
```

## Recommended Fix
Consider using a consistent error handling strategy:

**Option 1 - Use ESP_ERROR_CHECK for boot-critical operations:**
```cpp
// In main.cpp, keep ESP_ERROR_CHECK for NVS since it's boot-critical
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);
```

**Option 2 - Use manual checking for all HAL operations:**
```cpp
// In main.cpp, change to manual checking for consistency with HAL
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    esp_err_t eraseRet = nvs_flash_erase();
    if (eraseRet != ESP_OK) {
        LOG_E(TAG, "NVS erase failed: %s", esp_err_to_name(eraseRet));
        return;  // Or handle appropriately
    }
    ret = nvs_flash_init();
}
if (ret != ESP_OK) {
    LOG_E(TAG, "NVS init failed: %s", esp_err_to_name(ret));
    return;
}
```

**Option 3 - Add a helper macro for consistent error logging:**
```cpp
#define CHECK_ESP(err, msg) \
    do { \
        esp_err_t __err = (err); \
        if (__err != ESP_OK) { \
            LOG_E(TAG, "%s: %s", msg, esp_err_to_name(__err)); \
        } \
    } while (0)

// Usage:
CHECK_ESP(nvs_flash_init(), "NVS init");
```

## References
- ESP-IDF error handling best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/error-handling.html
- `ESP_ERROR_CHECK` documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/utils.html#macro-esp-error-check
