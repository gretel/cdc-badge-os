---
title: "[MEDIUM] Hardcoded hardware pin dependencies in modules"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "hardware-coupling"
  - "config-hardcoding"
---

## Summary
The `grove_led` module (and likely other modules) directly includes and hardcodes hardware pin definitions from `hw_config.h`, creating tight coupling between module logic and specific hardware configuration.

**Evidence:**
- `components/grove_led/src/GroveLedModule.cpp:4` includes `led_strip.h` and uses `GROVE_DATA_PIN` from `hw_config.h`
- Line 116: `GROVE_DATA_PIN` is used directly in LED strip configuration
- Line 42: `static constexpr gpio_num_t GROVE_DATA_PIN = GPIO_NUM_2;` in header duplicates the hardware config

## Impact
1. **Hardware portability**: The module cannot be easily adapted to different hardware configurations without code changes
2. **Testing difficulty**: Hardware-specific constants make unit testing harder
3. **Maintenance burden**: Changing pin assignments requires modifying module source code instead of just configuration
4. **Violation of HAL principle**: Modules should depend on hardware abstraction, not concrete pin definitions

## Evidence
File: `components/grove_led/src/GroveLedModule.cpp`
```cpp
// Line 4: Hardware-specific include
#include "led_strip.h"

// Line 116: Direct use of hardware pin
led_strip_config_t strip_config = {
    .strip_gpio_num = GROVE_DATA_PIN,  // Hardcoded dependency
    .max_leds = MAX_LEDS,
    ...
};
```

File: `components/grove_led/include/grove_led/GroveLedModule.h`
```cpp
// Line 42: Duplicates hardware config in module
static constexpr gpio_num_t GROVE_DATA_PIN = GPIO_NUM_2;
```

## Recommended Fix
1. Create a hardware abstraction for Grove/GPIO ports in `cdc_hal`:
   - Add `IGpioPort` interface with methods like `configureOutput()`, `write()`, etc.
   - Implement `GrovePort` class that wraps the actual GPIO pins

2. Inject the GPIO abstraction into the module:
   ```cpp
   class GroveLedModule : public core::IModule {
   public:
       void setGpioPort(cdc::hal::IGpioPort* port);  // Dependency injection
       // ...
   };
   ```

3. Update module initialization to use the injected port instead of hardcoded pins

This approach follows the existing HAL pattern used for Display, Keypad, and Secure Element.

## References
- Project HAL pattern: `components/cdc_hal/include/cdc_hal/IDisplay.h`
- Hardware config: `components/cdc_hal/include/cdc_hal/hw_config.h`
- Module architecture guidelines in `components/cdc_core/include/cdc_core/IModule.h`
