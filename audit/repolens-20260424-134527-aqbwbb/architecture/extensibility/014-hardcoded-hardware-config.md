---
title: "[HIGH] Hardcoded Hardware Configuration in hw_config.h"
severity: HIGH
domain: architecture/extensibility
lens: hardware-configuration
labels:
  - "audit:architecture/extensibility"
---

## Summary
Hardware configuration (pin definitions, I2C addresses, SPI pins) is hardcoded in `components/cdc_hal/include/cdc_hal/hw_config.h` as preprocessor `#define` macros. This makes it difficult to support hardware variants or runtime configuration without recompilation.

**Files affected:**
- `components/cdc_hal/include/cdc_hal/hw_config.h` (lines 1-53)

## Impact
- **Hardware variant support**: Different PCB revisions or board variants require forking the config file
- **Testing flexibility**: Cannot easily test with different pin assignments
- **Runtime configuration**: No way to detect and adapt to hardware differences at boot
- **Module portability**: Modules depend on specific pin assignments, making them less reusable

## Evidence
The file contains 53 lines of hardcoded definitions:

```cpp
// components/cdc_hal/include/cdc_hal/hw_config.h:8-53
#define I2C0_SDA_PIN GPIO_NUM_17
#define I2C0_SCL_PIN GPIO_NUM_18
#define I2C1_SDA_PIN GPIO_NUM_47
#define I2C1_SCL_PIN GPIO_NUM_48
#define EXPANDER_ADDR 0x20
#define BQ25895_ADDR 0x6A
#define EPD_CS_PIN GPIO_NUM_41
#define EPD_DC_PIN GPIO_NUM_45
#define TR01_CS_PIN GPIO_NUM_10
// ... more hardcoded pins
```

These are used throughout the codebase:
- `components/cdc_hal/src/I2cBus.cpp` uses `I2C0_SDA_PIN`, `I2C0_SCL_PIN`
- `components/cdc_hal/src/SpiBus.cpp` uses `SPI_SCLK_PIN`, `SPI_MISO_PIN`, `SPI_MOSI_PIN`
- `components/cdc_hal/src/TCA9535Keypad.cpp` uses `EXPANDER_ADDR`

## Recommended Fix

### Option 1: Kconfig-based Configuration (ESP32 native)
Create a `Kconfig.projbuild` file with configuration options:

```kconfig
config I2C0_SDA_PIN
    int "I2C0 SDA Pin"
    default 17
    range 0 48

config I2C0_SCL_PIN
    int "I2C0 SCL Pin"
    default 18
    range 0 48

config EXPANDER_ADDR
    hex "IO Expander I2C Address"
    default 0x20
```

Then access via `CONFIG_I2C0_SDA_PIN` in code.

### Option 2: Runtime Configuration Structure
Replace `#define` macros with a configurable structure:

```cpp
// components/cdc_hal/include/cdc_hal/HwConfig.h
struct HwConfig {
    gpio_num_t i2c0SdaPin;
    gpio_num_t i2c0SclPin;
    uint8_t expanderAddr;
    // ...
};

class HwConfigProvider {
public:
    virtual ~HwConfigProvider() = default;
    virtual const HwConfig& getConfig() const = 0;
};

// Default implementation
class DefaultHwConfig : public HwConfigProvider {
    const HwConfig config_ = {
        .i2c0SdaPin = GPIO_NUM_17,
        .i2c0SclPin = GPIO_NUM_18,
        // ...
    };
};
```

### Option 3: NVS-based Runtime Override
Allow runtime override via NVS:
```cpp
class ConfigurableHwConfig : public HwConfigProvider {
    HwConfig config_;
    
    void loadFromNvs() {
        nvs_handle_t nvs;
        if (nvs_open("hw", NVS_READONLY, &nvs) == ESP_OK) {
            // Load overrides
            size_t len;
            if (nvs_get_str(nvs, "i2c0_sda", buf, &len) == ESP_OK) {
                config_.i2c0SdaPin = (gpio_num_t)atoi(buf);
            }
            nvs_close(nvs);
        }
    }
};
```

## References
- ESP-IDF Kconfig documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/kconfig.html
- Strategy Pattern: https://refactoring.guru/design-patterns/strategy
- Open/Closed Principle: https://en.wikipedia.org/wiki/Open%E2%80%93closed_principle

</content>