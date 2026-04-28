---
title: "[MEDIUM] Missing error logging in HAL components"
severity: MEDIUM
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
Hardware Abstraction Layer (HAL) components in `components/cdc_hal/src/` have missing error logging for critical hardware initialization and operation failures.

**Key locations:**

1. **TROPIC01 Secure Element** (`Tropic01Element.cpp`): Missing detailed error logging for session management and operations

2. **I2C Bus** (`I2cBus.cpp`): Missing error logging for read/write operations

3. **SPI Bus** (`SpiBus.cpp`): Missing error logging for transactions

4. **Power Manager** (`BQ25895Power.cpp`): Missing logging for battery status updates and charger IRQ handling

5. **Display** (`EpaperDisplay.cpp`): Missing logging for display refresh failures

## Impact
- **Hardware debugging**: Hard to diagnose hardware communication failures
- **Error tracking**: Hardware errors don't appear in structured error log
- **Maintenance**: Difficult to identify intermittent hardware issues without log history

## Evidence
Looking at the main.cpp initialization (lines 99-198):
```cpp
LOG_I(TAG, "Initializing I2C bus...");
s_i2cBus = cdc::hal::getI2cBus0();
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
} else {
    LOG_E(TAG, "I2C bus init failed!");  // Only init is logged
}
```

The main loop has logging for init failures, but the HAL components themselves don't log operation-level errors.

## Recommended Fix
Add error logging to HAL component methods:

1. **I2C Bus** - Add to read/write methods:
```cpp
bool I2cBus::read(uint8_t addr, uint8_t* buffer, size_t len) {
    esp_err_t err = i2c_master_transmit(...);
    if (err != ESP_OK) {
        LOG_E(TAG, "I2C read failed for 0x%02X: %s", addr, esp_err_to_name(err));
        return false;
    }
    return true;
}
```

2. **SPI Bus** - Add to transaction methods:
```cpp
bool SpiBus::transfer(uint8_t* tx, uint8_t* rx, size_t len) {
    esp_err_t err = spi_device_transmit(...);
    if (err != ESP_OK) {
        LOG_E(TAG, "SPI transfer failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
```

3. **Secure Element** - Add to session methods:
```cpp
bool Tropic01Element::sessionStart() {
    lt_result_t result = lt_session_start(...);
    if (result != LT_OK) {
        LOG_E(TAG, "TROPIC01 session start failed: %d", result);
        return false;
    }
    return true;
}
```

## References
- `components/cdc_hal/src/` - HAL component implementations
- `components/cdc_hal/include/cdc_hal/` - HAL interfaces
