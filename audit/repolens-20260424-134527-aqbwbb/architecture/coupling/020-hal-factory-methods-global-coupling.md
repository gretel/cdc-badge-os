---
title: "[MEDIUM] HAL factory methods create hidden global coupling"
severity: MEDIUM
domain: architecture/coupling
lens: hal-factory-methods
labels:
  - "audit:architecture/coupling"
---

## Summary
HAL interfaces use factory functions like `getI2cBus0()`, `getPowerManagerInstance()`, `getDisplayInstance()`, etc. that return pointers to statically-allocated singletons. These functions are called from `main.cpp` during boot, but the implementations are in separate HAL components. This creates implicit coupling: any module can call these functions directly, bypassing the ServiceRegistry.

**Evidence:**
- `components/cdc_hal/include/cdc_hal/II2cBus.h` (line 40): `II2cBus* getI2cBus0();`
- `components/cdc_hal/include/cdc_hal/IPowerManager.h` (line 92): `IPowerManager* getPowerManagerInstance();`
- `components/cdc_hal/include/cdc_hal/IKeypad.h` (line 101): `IKeypad* getKeypadInstance();`
- `main/main.cpp` (lines 108-198): Calls all factory functions and stores in static variables

## Impact
**Bypasses dependency injection:** Modules can (and do) call these factory functions directly instead of using ServiceRegistry. This creates hidden dependencies that aren't visible in the module's interface.

**Example from codebase:**
```cpp
// main.cpp (lines 44-48) - Static globals
static cdc::hal::II2cBus* s_i2cBus = nullptr;
static cdc::hal::IKeypad* s_keypad = nullptr;
static cdc::hal::IPowerManager* s_powerManager = nullptr;

// Any file can call:
auto* keypad = cdc::hal::getKeypadInstance();  // No dependency declared!
```

**Hard to test:** To test a module, you must ensure the HAL singletons are initialized first. Can't easily mock dependencies.

**Inconsistent patterns:** Some services use ServiceRegistry (`ServiceRegistry::instance().registerService()`), others use factory functions. New developers don't know which to use.

## Evidence
**File: `components/cdc_hal/include/cdc_hal/II2cBus.h` (lines 38-42)**
```cpp
/**
 * Get I2C bus 0 instance
 * @return I2C bus 0 pointer (BQ25895 + TCA9535)
 */
II2cBus* getI2cBus0();  // BQ25895 + TCA9535
```

**File: `main/main.cpp` (lines 108-112)**
```cpp
LOG_I(TAG, "Initializing I2C bus...");
s_i2cBus = cdc::hal::getI2cBus0();
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
} else {
    LOG_E(TAG, "I2C bus init failed!");
}
```

**File: `components/cdc_hal/src/I2cBus.cpp` (implementation)**
```cpp
static I2cBus0 g_i2cBus0;  // Static singleton

I2cBus* getI2cBus0() {
    return &g_i2cBus0;  // Returns pointer to static
}
```

## Recommended Fix
**Option 1: Use ServiceRegistry consistently**
Remove factory functions. All HAL services should be registered with ServiceRegistry:
```cpp
// In main.cpp
auto* i2cBus = new I2cBus0();
ServiceRegistry::instance().registerService("i2c_bus_0", i2cBus);

// In modules
auto* i2cBus = ServiceRegistry::instance().get<II2cBus>("i2c_bus_0");
```

**Option 2: Factory methods as ServiceRegistry wrappers**
Make factory functions delegate to ServiceRegistry:
```cpp
II2cBus* getI2cBus0() {
    return ServiceRegistry::instance().get<II2cBus>("i2c_bus_0");
}
```

**Option 3: Dependency injection via constructor**
Pass required HAL services to module constructors (requires changing module interface).

## References
- `components/cdc_hal/include/cdc_hal/II2cBus.h` - I2C factory
- `components/cdc_hal/include/cdc_hal/IPowerManager.h` - Power factory
- `components/cdc_hal/include/cdc_hal/IKeypad.h` - Keypad factory
- `main/main.cpp` - HAL initialization
- `components/cdc_core/include/cdc_core/ServiceRegistry.h` - Service registry
