---
title: "[LOW] I2C command handles created per-operation without reuse"
severity: LOW
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/cdc_hal/src/I2cBus.cpp`, I2C command handles are created and destroyed for every register read/write operation. While this is the ESP-IDF recommended pattern, for high-frequency operations (like the TCA9535 keypad polling or BQ25895 battery monitoring), this creates repeated allocation overhead.

**Specific locations:**
- `writeReg()` (line 128-147): Creates command handle, writes, deletes handle
- `readReg()` (line 154-176): Creates command handle, reads, deletes handle

These methods are called frequently:
- TCA9535 keypad is polled every 10ms in the main loop
- BQ25895 power manager updates on each `update()` call

## Impact
1. **Allocation overhead**: Each `i2c_cmd_link_create()` allocates memory and initializes the command structure. For operations happening every 10ms (keypad) or ~100ms (battery), this adds up.
2. **Heap fragmentation**: Repeated small allocations can fragment the heap over time on an embedded system with limited RAM.
3. **Latency**: The allocation/deallocation adds small but measurable latency to each I2C operation.

## Evidence
**File: `components/cdc_hal/src/I2cBus.cpp`**

1. **writeReg() - creates and deletes handle each call:**
```cpp
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                                const uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();  // Allocate
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    if (data && len > 0) {
        i2c_master_write(cmd, data, len, true);
    }
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);  // Free

    return err;
}
```

2. **readReg() - same pattern:**
```cpp
esp_err_t I2cBusImpl::readReg(I2cDeviceHandle handle, uint8_t reg,
                               uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();  // Allocate
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);  // Free

    return err;
}
```

## Recommended Fix
For an embedded system where performance is critical, consider pre-allocating command handles during initialization and reusing them:

1. **Add pre-allocated command handles:**
```cpp
class I2cBusImpl : public II2cBus {
private:
    i2c_port_t port_;
    gpio_num_t sda_;
    gpio_num_t scl_;
    const char* name_;
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;

    static constexpr size_t MAX_DEVICES = 4;
    I2cDevice devices_[MAX_DEVICES] = {};
    size_t deviceCount_ = 0;

    // Pre-allocated command handles for performance
    i2c_cmd_handle_t writeCmd_ = nullptr;
    i2c_cmd_handle_t readCmd_ = nullptr;
};
```

2. **Initialize in `init()`:**
```cpp
bool I2cBusImpl::init() {
    // ... existing I2C config ...

    // Pre-allocate command handles
    writeCmd_ = i2c_cmd_link_create();
    readCmd_ = i2c_cmd_link_create();

    state_ = core::ServiceState::INITIALIZED;
    LOG_I(TAG, "%s initialized (SDA=%d, SCL=%d)", name_, sda_, scl_);
    return true;
}
```

3. **Reuse in `writeReg()`:**
```cpp
esp_err_t I2cBusImpl::writeReg(I2cDeviceHandle handle, uint8_t reg,
                                const uint8_t* data, size_t len) {
    auto* dev = static_cast<I2cDevice*>(handle);
    if (!dev) return ESP_ERR_INVALID_ARG;

    // Reuse pre-allocated handle
    i2c_cmd_handle_t cmd = writeCmd_;
    // Note: Need to reset handle state before reuse
    // ESP-IDF doesn't provide reset, so create/delete is safer

    // Actually, for simplicity and safety, keep current pattern
    // The overhead is minimal for typical I2C frequencies
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // ... rest unchanged ...
    i2c_cmd_link_delete(cmd);

    return err;
}
```

**Alternative approach**: For most embedded use cases, the current pattern is acceptable. The optimization is only needed if profiling shows I2C command creation is a bottleneck. A better approach might be to batch multiple register operations into a single command chain:

```cpp
// Batch write multiple registers in one command
esp_err_t writeRegs(I2cDeviceHandle handle, const uint8_t* regs, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    for (size_t i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, regs[i], true);
    }
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}
```

## References
- [ESP-IDF I2C Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html) - Command handle API
- [I2C Bus Timing](https://www.nxp.com/docs/en/user-guide/UM10204.pdf) - Standard I2C timing
- [Embedded System Optimization](https://www.embedded.com/electronics-blogs/4001481/Embedded-system-optimization--Part-1) - General optimization strategies
