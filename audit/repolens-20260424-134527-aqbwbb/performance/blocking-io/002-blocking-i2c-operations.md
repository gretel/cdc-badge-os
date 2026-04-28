---
title: "[LOW] Blocking I2C operations without async alternatives"
severity: LOW
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "i2c"
  - "hal"
---

## Summary

The I2C bus implementation (`components/cdc_hal/src/I2cBus.cpp`) uses blocking `i2c_master_cmd_begin()` for all transfers. While I2C is inherently slower than SPI, blocking operations can still cause delays, especially when multiple devices share the bus or when devices are slow to respond.

**Affected file:**
- `components/cdc_hal/src/I2cBus.cpp` (lines 142, 176)

**Evidence:**
```cpp
// components/cdc_hal/src/I2cBus.cpp:142
esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
```

```cpp
// components/cdc_hal/src/I2cBus.cpp:176
esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
```

## Impact

1. **Device read/write blocking**: Each I2C transaction blocks until completion or timeout (100ms default).

2. **Slow device response**: If a device is slow (e.g., sensor conversion time), the entire I2C bus is blocked.

3. **Bus contention**: Multiple devices on the same I2C bus serialize, increasing latency for all.

4. **Timeout delays**: Failed transactions wait the full timeout (100ms) before returning.

## Evidence

**Current implementation:**
```cpp
// components/cdc_hal/src/I2cBus.cpp:128-146
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                               const uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    if (data && len > 0) {
        i2c_master_write(cmd, data, len, true);
    }
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return err;
}
```

**Devices on I2C bus (from hw_config.h):**
- BQ25895 (charger)
- TROPIC01 (secure element - also uses SPI)
- RTC (DS3231 or similar)

## Recommended Fix

1. **Use I2C driver with transactionqueuing**:
   ```cpp
   // Queue multiple I2C transactions
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
   i2c_master_write(cmd, buffer, len, true);
   i2c_master_stop(cmd);
   
   // Use non-blocking submit if available
   i2c_master_cmd_begin_nonblocking(port, cmd, callback, user_data);
   ```

2. **Reduce timeout for fast devices**:
   ```cpp
   // Different timeouts per device type
   static constexpr uint32_t RTC_TIMEOUT_MS = 50;
   static constexpr uint32_t CHARGER_TIMEOUT_MS = 100;
   static constexpr uint32_t SENSOR_TIMEOUT_MS = 200;
   ```

3. **Batch I2C reads**:
   ```cpp
   // Read multiple registers in one transaction
   i2c_master_read(cmd, data, len, I2C_MASTER_NACK);
   // Instead of individual register reads
   ```

4. **Add retry logic with backoff**:
   ```cpp
   // Retry on failure with increasing delays
   for (int i = 0; i < 3; i++) {
       err = i2c_master_cmd_begin(port, cmd, timeout);
       if (err == ESP_OK) break;
       vTaskDelay(pdMS_TO_TICKS(5 * (1 << i)));
   }
   ```

**Estimated effort**: 30-60 minutes for timeout tuning and basic optimizations

## References

- [ESP32 I2C Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/peripherals/i2c.html)
- I2C typical speeds: 100kHz (standard), 400kHz (fast)
- Typical I2C transaction: 1-10ms depending on data size and device speed
