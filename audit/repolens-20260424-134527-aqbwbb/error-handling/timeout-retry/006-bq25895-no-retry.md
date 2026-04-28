---
title: "[MEDIUM] BQ25895 power manager I2C operations lack retry logic for transient failures"
severity: MEDIUM
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "power-management"
  - "i2c"
  - "retry"
  - "bq25895"
---

## Summary
The BQ25895 power manager (`components/cdc_hal/src/BQ25895Power.cpp`) performs I2C register reads/writes without any retry logic for transient failures. A single I2C bus glitch or temporary device unavailability can cause power management operations to fail silently.

**Evidence:**

1. File: `components/cdc_hal/src/BQ25895Power.cpp:138-144` - Single-read helper with no retry:
   ```cpp
   bool BQ25895Power::readReg(uint8_t reg, uint8_t* value) const {
       if (!device_ || !value) return false;
       return bus_->readReg(device_, reg, value, 1) == ESP_OK;  // Single attempt!
   }
   ```

2. File: `components/cdc_hal/src/BQ25895Power.cpp:147-151` - Single-write helper with no retry:
   ```cpp
   bool BQ25895Power::writeReg(uint8_t reg, uint8_t value) {
       if (!device_) return false;
       return bus_->writeReg(device_, reg, &value, 1) == ESP_OK;  // Single attempt!
   }
   ```

3. File: `components/cdc_hal/src/BQ25895Power.cpp:154-176` - `updateRegBits()` calls read/write helpers:
   ```cpp
   bool BQ25895Power::updateRegBits(uint8_t reg, uint8_t mask, uint8_t value, const char* label) {
       uint8_t current = 0;
       if (!readReg(reg, &current)) {
           LOG_E(TAG, "Read failed: %s", label);
           return false;  // No retry!
       }
       // ...
       if (!writeReg(reg, newVal)) {
           LOG_E(TAG, "Write failed: %s", label);
           return false;  // No retry!
       }
       // ...
   }
   ```

4. File: `components/cdc_hal/src/BQ25895Power.cpp:214-229` - Initialization reads vendor register once:
   ```cpp
   uint8_t vendor = 0;
   if (!readReg(BQ_REG_VENDOR, &vendor)) {
       LOG_E(TAG, "Failed to read vendor register");
       state_ = core::ServiceState::ERROR;
       return false;  // No retry on boot!
   }
   ```

## Impact
- **Power management instability**: Transient I2C errors can leave charger in unknown state
- **Silent failures**: Battery voltage/percentage may be stale if read fails
- **Boot reliability**: Single I2C glitch during init can prevent charger initialization
- **Watchdog misses**: If `kickWatchdog()` fails silently, charger watchdog can expire
- **Charging control**: Charge current settings may not apply correctly

## Recommended Fix
Add retry logic with exponential backoff for transient I2C failures:

1. Add retry constants:
   ```cpp
   static constexpr uint8_t I2C_RETRY_COUNT = 3;
   static constexpr uint32_t I2C_RETRY_DELAY_MS = 5;
   ```

2. Add retry wrappers:
   ```cpp
   bool BQ25895Power::readRegWithRetry(uint8_t reg, uint8_t* value, uint8_t maxRetries) const {
       for (uint8_t i = 0; i < maxRetries; i++) {
           if (bus_->readReg(device_, reg, value, 1) == ESP_OK) {
               return true;
           }
           if (i < maxRetries - 1) {
               vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
           }
       }
       return false;
   }

   bool BQ25895Power::writeRegWithRetry(uint8_t reg, uint8_t value, uint8_t maxRetries) {
       for (uint8_t i = 0; i < maxRetries; i++) {
           if (bus_->writeReg(device_, reg, &value, 1) == ESP_OK) {
               return true;
           }
           if (i < maxRetries - 1) {
               vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
           }
       }
       return false;
   }
   ```

3. Replace critical operations with retry versions:
   ```cpp
   // In init() - vendor register read
   if (!readRegWithRetry(BQ_REG_VENDOR, &vendor, I2C_RETRY_COUNT)) {
       LOG_E(TAG, "Failed to read vendor register after %d retries", I2C_RETRY_COUNT);
       state_ = core::ServiceState::ERROR;
       return false;
   }

   // In updateRegBits()
   if (!readRegWithRetry(reg, &current, I2C_RETRY_COUNT)) {
       LOG_E(TAG, "Read failed after retry: %s", label);
       return false;
   }

   // In kickWatchdog()
   if (readRegWithRetry(BQ_REG_CHG_CTRL, &current, I2C_RETRY_COUNT)) {
       writeRegWithRetry(BQ_REG_CHG_CTRL, current | (1 << 6), I2C_RETRY_COUNT);
   }
   ```

4. Consider adding a "device healthy" flag that gets cleared on repeated failures

## References
- I2C bus transient failure patterns: https://www.ti.com/lit/an/slva704/slva704.pdf
- ESP32 I2C troubleshooting: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- Power management reliability best practices

</content>