---
title: "[MEDIUM] Hardcoded hardware configuration in source files"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
Hardware-specific configuration (I2C addresses, SPI bus numbers, frequencies) is hardcoded in source files rather than being centralized in a hardware configuration header. This makes it difficult to support hardware variants or adjust parameters without code changes.

## Impact
1. **Hardware variant support**: Cannot easily support hardware revisions with different pinouts
2. **Debug difficulty**: Must search source files to find hardware parameters
3. **Build configuration**: Some hardware config is in Kconfig (sdkconfig), some in headers, creating confusion
4. **Maintenance**: Changing hardware requires finding all hardcoded values

## Evidence
**Hardware configuration scattered across files:**

| Configuration | Value | Location |
|--------------|-------|----------|
| I2C0 SDA | GPIO 17 | `components/cdc_hal/include/cdc_hal/hw_config.h:10` |
| I2C0 SCL | GPIO 18 | `components/cdc_hal/include/cdc_hal/hw_config.h:11` |
| I2C1 SDA | GPIO 47 | `components/cdc_hal/include/cdc_hal/hw_config.h:14` |
| I2C1 SCL | GPIO 48 | `components/cdc_hal/include/cdc_hal/hw_config.h:15` |
| I2C0 frequency | 100kHz | `components/cdc_hal/src/I2cBus.cpp:18` |
| I2C timeout | 100ms | `components/cdc_hal/src/I2cBus.cpp:19` |
| SPI bus | SPI2_HOST | `components/cdc_hal/src/SpiBus.cpp:15` |
| I2C0 devices | 0x20, 0x6A | `components/cdc_hal/include/cdc_hal/hw_config.h:18,22` |
| SAO EEPROM | 0x50 | `components/cdc_hal/include/cdc_hal/hw_config.h:49` |
| E-Paper SPI | Kconfig | `sdkconfig.defaults:51-59` |

**Code examples:**
```cpp
// components/cdc_hal/src/I2cBus.cpp:18-19 (hardcoded, not in hw_config.h)
static constexpr uint32_t I2C_FREQ_HZ = 100000;  // 100kHz standard mode
static constexpr uint32_t I2C_TIMEOUT_MS = 100;

// components/cdc_hal/src/SpiBus.cpp:15 (hardcoded)
static constexpr spi_host_device_t SPI_BUS_HOST = SPI2_HOST;

// components/cdc_hal/include/cdc_hal/hw_config.h:18 (device addresses)
#define EXPANDER_ADDR 0x20
#define BQ25895_ADDR 0x6A
#define SAO_EEPROM_ADDR 0x50
```

**Mixed configuration sources:**
- E-Paper display pins are in `sdkconfig.defaults` (Kconfig)
- Other pins are in `hw_config.h` (C header)
- No unified documentation

## Recommended Fix
1. **Consolidate hardware configuration in `hw_config.h`**:
   ```cpp
   // components/cdc_hal/include/cdc_hal/hw_config.h
   #pragma once
   #include "driver/gpio.h"
   
   // === I2C Configuration ===
   #define I2C0_FREQ_HZ 100000
   #define I2C0_TIMEOUT_MS 100
   #define I2C1_FREQ_HZ 100000
   #define I2C1_TIMEOUT_MS 100
   
   // I2C0 devices
   #define I2C0_EXPANDER_ADDR 0x20
   #define I2C0_BQ25895_ADDR 0x6A
   #define I2C0_TCA9535_ADDR 0x20
   
   // I2C1 devices (expansion header)
   #define I2C1_SAO_EEPROM_ADDR 0x50
   
   // === SPI Configuration ===
   #define SPI_HOST SPI2_HOST
   #define SPI_FREQ_HZ 10000000  // 10MHz
   
   // === E-Paper Configuration (from Kconfig) ===
   #ifndef CONFIG_EINK_SPI_MOSI
   #define EPD_MOSI_PIN GPIO_NUM_13
   #else
   #define EPD_MOSI_PIN CONFIG_EINK_SPI_MOSI
   #endif
   // ... (other pins)
   ```

2. **Update source files to use config**:
   ```cpp
   // components/cdc_hal/src/I2cBus.cpp
   #include "cdc_hal/hw_config.h"
   
   static constexpr uint32_t I2C_FREQ_HZ = I2C0_FREQ_HZ;
   static constexpr uint32_t I2C_TIMEOUT_MS = I2C0_TIMEOUT_MS;
   ```

3. **Create documentation** for hardware configuration:
   - Add a `docs/HARDWARE_CONFIGURATION.md` explaining all hardware parameters
   - Document how to change pins for custom hardware

## References
- [ESP-IDF Pin Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html)
- [Hardware Abstraction Patterns](https://www.oreilly.com/library/view/embedded-systems-interfacing/9780137043665/ch01.html)
