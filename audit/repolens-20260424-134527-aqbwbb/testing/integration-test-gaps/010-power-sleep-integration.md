---
title: "[MEDIUM] Power manager and sleep controller integration lacks tests"
severity: MEDIUM
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_hal"
  - "area:power"
---

## Summary
The power manager (BQ25895) and sleep controller components manage battery charging and low-power modes, but **no integration tests** verify that power events are correctly detected and modules respond appropriately.

## Evidence

**IPowerManager API** (`components/cdc_hal/include/cdc_hal/IPowerManager.h`):
```cpp
class IPowerManager {
    uint16_t getBatteryVoltage();
    uint8_t getBatteryPercent();
    bool isCharging();
    void update();  // Called from main loop
};
```

**ISleepController API** (`components/cdc_hal/include/cdc_hal/ISleepController.h`):
```cpp
class ISleepController {
    bool enterLightSleep();
    bool enterDeepSleep(uint64_t durationMs);
    uint32_t getLightSleepInterval();
};
```

**Implementation** (`components/cdc_hal/src/BQ25895Power.cpp`, `SleepController.cpp`):
- BQ25895 charger via I2C
- IRQ handling for charge done/low battery
- ESP32-S3 light/deep sleep APIs

**Event integration** (`components/cdc_core/include/cdc_core/EventBus.h:18-24`):
```cpp
enum class EventType {
    POWER_USB_CONNECTED,
    POWER_USB_DISCONNECTED,
    POWER_CHARGING,
    POWER_BATTERY_LOW,
    POWER_BATTERY_CRITICAL,
};
```

**Usage in main** (`main/main.cpp:111-126, 131-140`):
```cpp
s_powerManager = cdc::hal::getPowerManagerInstance();
s_powerManager->init();
s_powerManager->start();

s_sleepController = cdc::hal::getSleepControllerInstance();
s_sleepController->init();
s_sleepController->start();
```

**Module integration** (`components/mod_*/*Module.cpp`):
- Modules receive `onUsbConnect()`, `onUsbDisconnect()` events
- No tests verify this integration

**Current test coverage**: None

## Impact
- **Battery monitoring**: Low battery detection may fail
- **Charging events**: Charge complete may not trigger UI update
- **Sleep timing**: Light sleep interval not optimized
- **Wake handling**: Modules may not recover correctly from sleep

## Recommended Fix

Create integration test `test_power_sleep_integration/` that verifies:

1. **Battery reading**: Voltage and percentage read correctly
2. **Charging detection**: Charging state detected from IRQ
3. **Power events**: USB connect/disconnect events published
4. **Sleep entry**: Light sleep enters and wakes correctly
5. **Module response**: Modules receive power events

**Test structure** (example):
```cpp
// test/test_power_sleep_integration/test_power_manager.cpp
#include "cdc_hal/IPowerManager.h"
#include "cdc_core/EventBus.h"

void test_battery_reading() {
    IPowerManager* power = getPowerManagerInstance();
    power->init();
    
    uint16_t voltage = power->getBatteryVoltage();
    uint8_t percent = power->getBatteryPercent();
    
    ASSERT_GT(voltage, 3000);  // > 3.0V
    ASSERT_LE(percent, 100);
}

void test_power_events() {
    bool usbConnected = false;
    EventBus::instance().subscribe([](const Event& e) {
        if (e.type == EventType::POWER_USB_CONNECTED) {
            usbConnected = true;
        }
    });
    
    // Simulate USB connect (hardware-dependent)
    power->update();
    
    ASSERT_TRUE(usbConnected);
}
```

## References
- [IPowerManager interface](components/cdc_hal/include/cdc_hal/IPowerManager.h)
- [ISleepController interface](components/cdc_hal/include/cdc_hal/ISleepController.h)
- [BQ25895Power implementation](components/cdc_hal/src/BQ25895Power.cpp)
- [SleepController implementation](components/cdc_hal/src/SleepController.cpp)
