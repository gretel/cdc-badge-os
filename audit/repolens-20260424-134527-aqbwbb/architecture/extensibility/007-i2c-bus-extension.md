---
title: "[MEDIUM] I2C Bus Implementation is Hardcoded to Single Bus"
severity: MEDIUM
domain: extensibility
lens: hardware-abstraction
labels:
  - "audit:architecture/extensibility"
---

## Summary
The I2C bus system in `components/cdc_hal/src/I2cBus.cpp` and `components/cdc_hal/include/cdc_hal/II2cBus.h` assumes a single I2C bus (bus 0). Adding additional I2C buses (e.g., for expansion headers) requires modifying the HAL implementation.

**Evidence:**
- `components/cdc_hal/include/cdc_hal/II2cBus.h`:
  ```cpp
  class II2cBus {
  public:
      virtual bool init();
      virtual bool transfer(uint8_t addr, const uint8_t* tx, size_t txLen,
                            uint8_t* rx, size_t rxLen);
  };
  
  // Factory function - hardcoded to bus 0
  II2cBus* getI2cBus0();
  ```

- `components/cdc_hal/src/I2cBus.cpp`:
  ```cpp
  II2cBus* getI2cBus0() {
      static I2cBusImpl bus0;  // Singleton for bus 0 only
      return &bus0;
  }
  ```

- `components/cdc_os_ui/src/AppUi.cpp:108`:
  ```cpp
  s_i2cBus = cdc::hal::getI2cBus0();
  ```

## Impact
**Hardware Limitation:** Adding a second I2C bus requires:
1. Modifying `II2cBus.h` to add `getI2cBus1()`
2. Creating new implementation class
3. Updating all I2C device drivers to accept bus parameter
4. No dynamic bus discovery for expansion modules

## Evidence
Files affected:
- `components/cdc_hal/include/cdc_hal/II2cBus.h` (interface)
- `components/cdc_hal/src/I2cBus.cpp` (implementation)
- `components/cdc_os_ui/src/AppUi.cpp:108` (usage)
- `components/cdc_hal/include/cdc_hal/hw_config.h` (pin definitions - hardcoded)

Hardcoded in `main/CMakeLists.txt:26`:
```cmake
idf_component_register(
    ...
    REQUIRES nvs_flash freertos esp_timer cdc_core usb_badge serial_cmd cdc_hal cdc_log cdc_os_ui
        ${MODULES}
)
```
I2C bus selection is compile-time via `hw_config.h`.

## Recommended Fix
Implement bus registry:

1. **Bus registry:**
   ```cpp
   class I2cBusRegistry {
   public:
       void registerBus(uint8_t id, II2cBus* bus);
       II2cBus* getBus(uint8_t id);
       uint8_t getBusCount();
   };
   ```

2. **Dynamic bus selection:**
   ```cpp
   // In module init
   auto* bus = I2cBusRegistry::instance().getBus(0);  // or 1, 2, etc.
   ```

3. **Or pass bus to devices:**
   ```cpp
   class EpaperDisplay : public IDisplay {
   public:
       EpaperDisplay(II2cBus* bus, uint8_t csPin);
   };
   ```

## References
- Bus architecture patterns
- Hardware abstraction layer design
