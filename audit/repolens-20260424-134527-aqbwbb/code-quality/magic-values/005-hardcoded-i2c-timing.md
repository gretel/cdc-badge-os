---
title: "[MEDIUM] Hardcoded I2C timing and device count constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Magic values for I2C bus timing (100kHz, 100ms timeout) and device capacity (4 devices) are used in `components/cdc_hal/src/I2cBus.cpp`. While some are defined as constants, the connection to hardware specifications could be clearer.

**Location:** `components/cdc_hal/src/I2cBus.cpp:16-17,57`

## Impact
- **Maintainability**: Different I2C devices may require different timing parameters.
- **Scalability**: The 4-device limit is arbitrary and may need adjustment.
- **Clarity**: Comments exist but could be more explicit about why these values were chosen.

## Evidence

**Lines 16-17:**
```cpp
static constexpr uint32_t I2C_FREQ_HZ = 100000;  // 100kHz standard mode
static constexpr uint32_t I2C_TIMEOUT_MS = 100;
```

**Line 57:**
```cpp
static constexpr size_t MAX_DEVICES = 4;
```

The timeout value of 100ms is used in I2C operations at lines 142 and 176:
```cpp
esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
```

## Recommended Fix

1. Enhance comments to explain the rationale:
```cpp
/** \brief I2C bus timing configuration constants. */
static constexpr uint32_t I2C_FREQ_HZ = 100000;  // Standard mode (BQ25895, TCA9535 support up to 400kHz)
static constexpr uint32_t I2C_TIMEOUT_MS = 100;  // Sufficient for short transfers, prevents hangs
```

2. Consider making device count configurable or increasing it with justification:
```cpp
/** \brief Maximum I2C devices per bus (current: BQ25895 + TCA9535 = 2, reserve 2 for expansion). */
static constexpr size_t MAX_DEVICES = 4;
```

3. Consider adding a higher-speed mode constant for devices that support 400kHz:
```cpp
static constexpr uint32_t I2C_FAST_MODE_HZ = 400000;  // Fast mode support
```

## References
- [I2C Bus Specification](https://www.nxp.com/docs/en/user-guide/UM10398.pdf)
- `components/cdc_hal/src/I2cBus.cpp` - I2C bus implementation
