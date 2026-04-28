---
title: "[018] [LOW] Repetitive null-check-and-call patterns"
severity: LOW
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The codebase uses repetitive `if (ptr && ptr->method())` patterns that could be simplified with helper functions or modern C++ features.

## Impact
- Verbose boilerplate obscures the actual logic
- Easy to miss a null check when copying patterns
- Inconsistent handling (some places check, some don't)

## Evidence
**File: `main/main.cpp` - Multiple occurrences:**

Lines 108-113:
```cpp
LOG_I(TAG, "Initializing I2C bus...");
s_i2cBus = cdc::hal::getI2cBus0();
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
} else {
    LOG_E(TAG, "I2C bus init failed!");
}
```

Lines 118-124:
```cpp
LOG_I(TAG, "Initializing Power Management...");
s_powerManager = cdc::hal::getPowerManagerInstance();
if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
    LOG_I(TAG, "Power Management ready (BQ25895)");
    LOG_I(TAG, "Battery: %d%% (%dmV)", s_powerManager->getBatteryPercent(),
          s_powerManager->getBatteryVoltage());
} else {
    LOG_E(TAG, "Power Management init failed!");
}
```

Lines 130-136:
```cpp
LOG_I(TAG, "Initializing Sleep Controller...");
s_sleepController = cdc::hal::getSleepControllerInstance();
if (s_sleepController && s_sleepController->init() && s_sleepController->start()) {
    LOG_I(TAG, "Sleep Controller ready (interval: %lus)",
          (unsigned long)s_sleepController->getLightSleepInterval());
} else {
    LOG_E(TAG, "Sleep Controller init failed!");
}
```

Lines 140-146:
```cpp
LOG_I(TAG, "Initializing WiFi Controller...");
cdc::hal::IWifiController* wifiController = cdc::hal::getWifiControllerInstance();
if (wifiController && wifiController->init() && wifiController->start()) {
    LOG_I(TAG, "WiFi Controller ready");
} else {
    LOG_E(TAG, "WiFi Controller init failed!");
}
```

This pattern repeats for Bluetooth, Keypad, Secure Element, and Display initialization.

**File: `components/cdc_os_ui/src/AppUi.cpp:196-200`**
```cpp
void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;
    bool usbConnected = s_deps.power->isUsbConnected();
    // ...
}
```

## Recommended Fix
Create a helper template for safe method chaining:

```cpp
// Helper for safe null-check-and-call
template<typename T, typename R>
R safe_call(T* ptr, R (T::*method)(), const char* context = "") {
    if (ptr) return (ptr->*method)();
    LOG_E("MAIN", "Null pointer in %s", context);
    return R{};  // Default value
}

// Usage:
s_i2cBus = cdc::hal::getI2cBus0();
if (s_i2cBus && s_i2cBus->init()) {
    LOG_I(TAG, "I2C bus ready");
}

// Or even better - use a service initializer pattern:
template<typename Service>
bool initService(Service*& instance, const char* name) {
    instance = Service::getInstance();
    if (instance && instance->init() && instance->start()) {
        LOG_I(TAG, "%s ready", name);
        return true;
    }
    LOG_E(TAG, "%s init failed!", name);
    return false;
}

// Usage:
if (!initService(s_i2cBus, "I2C bus")) return;
if (!initService(s_powerManager, "Power Management")) return;
if (!initService(s_sleepController, "Sleep Controller")) return;
```

## References
- C++ Core Guidelines: F.21 - Use function objects for simple callbacks
- Modern C++: "Use templates to reduce boilerplate"
