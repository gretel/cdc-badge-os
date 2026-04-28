---
title: "[MEDIUM] I2C bus lacks retry logic for transient communication failures"
severity: MEDIUM
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "i2c"
  - "hardware"
  - "retry"
---

## Summary
The I2C bus implementation (`components/cdc_hal/src/I2cBus.cpp`) has a 100ms timeout configured for read/write operations but lacks any retry logic for transient failures. I2C buses commonly experience transient issues like clock stretching, bus arbitration, or temporary device unresponsiveness that could be resolved with simple retry logic.

**Evidence:**
- File: `components/cdc_hal/src/I2cBus.cpp:18` - Timeout defined but no retry:
  ```cpp
  static constexpr uint32_t I2C_FREQ_HZ = 100000;  // 100kHz standard mode
  static constexpr uint32_t I2C_TIMEOUT_MS = 100;
  ```
- File: `components/cdc_hal/src/I2cBus.cpp:128-147` - Single attempt write:
  ```cpp
  esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
  ```
- File: `components/cdc_hal/src/I2cBus.cpp:157-178` - Single attempt read:
  ```cpp
  esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
  ```

## Impact
- **Reliability**: Transient I2C failures (common with E-Paper displays and sensors) cause immediate operation failure instead of recovery
- **User experience**: Device may appear unresponsive if I2C device momentarily stalls
- **Error handling**: No distinction between permanent vs. transient failures

## Recommended Fix
Add retry logic with exponential backoff to I2C read/write operations:

1. Define retry constants:
   ```cpp
   static constexpr uint8_t I2C_MAX_RETRY = 3;
   static constexpr uint32_t I2C_RETRY_DELAY_MS = 10;
   ```

2. Wrap `i2c_master_cmd_begin` calls in retry loop:
   ```cpp
   for (uint8_t attempt = 0; attempt < I2C_MAX_RETRY; attempt++) {
       esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
       if (err == ESP_OK) return ESP_OK;
       if (attempt < I2C_MAX_RETRY - 1) {
           vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
       }
   }
   ```

3. Log retry attempts at DEBUG level and failures at ERROR level

## References
- ESP-IDF I2C driver documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- I2C bus error recovery best practices: https://www.nxp.com/docs/en/user-guide/UM10204.pdf
