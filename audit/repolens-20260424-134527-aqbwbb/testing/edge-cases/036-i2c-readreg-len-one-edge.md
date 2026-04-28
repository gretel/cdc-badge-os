---
title: "[MEDIUM] I2cBus::readReg has edge case handling for len=1 that may cause issues"
severity: MEDIUM
domain: cdc_hal/I2cBus
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `I2cBus.cpp` (file: `components/cdc_hal/src/I2cBus.cpp:156-179`), the `readReg` function handles the edge case of reading exactly 1 byte, but the logic may be fragile when `len=1` is passed.

Lines 167-173:
```cpp
// Write register address
i2c_cmd_handle_t cmd = i2c_cmd_link_create();
i2c_master_start(cmd);
i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
i2c_master_write_byte(cmd, reg, true);

// Repeated start and read
i2c_master_start(cmd);
i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_READ, true);
if (len > 1) {
    i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
}
i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
i2c_master_stop(cmd);
```

When `len=1`:
- The `if (len > 1)` block at line 170 is skipped
- `i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK)` becomes `i2c_master_read_byte(cmd, data + 0, I2C_MASTER_NACK)`
- This should work, but the code path is different from `len > 1`

## Impact
- **Edge case fragility**: The `len=1` case uses `i2c_master_read_byte` while `len > 1` uses `i2c_master_read` followed by `i2c_master_read_byte`
- **Potential inconsistency**: Different I2C API paths may behave differently on edge cases
- **Testing gap**: The `len=1` case may not be exercised by standard tests

## Evidence
File: `components/cdc_hal/src/I2cBus.cpp`, lines 156-179

```cpp
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                              uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;  // Line 159

    // Write register address
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    // Repeated start and read
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);  // Line 170
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);  // Line 172
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return err;
}
```

The edge case at line 172 with `len=1`:
- `data + len - 1` = `data + 0` = `data` (correct)
- But the path differs from multi-byte reads

## Recommended Fix
Consolidate the read logic to handle all cases uniformly:

```cpp
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                              uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;

    // Write register address
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    // Repeated start and read
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_READ, true);
    
    // Read all bytes except the last one with ACK
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    
    // Read last byte with NACK
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return err;
}
```

Also add explicit unit tests for:
- `readReg(dev, reg, buffer, 1)` - single byte read
- `readReg(dev, reg, buffer, 2)` - two byte read (boundary)
- `readReg(dev, reg, buffer, 32)` - larger read

## References
- ESP-IDF I2C driver documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html
- I2C bus specification: https://www.nxp.com/docs/en/user-guide/UM10204.pdf
- CWE-398: Indicator of Poor Code Quality (edge case handling)
