---
title: "[MEDIUM] Inappropriate Intimacy: AppUi.cpp reaches into HAL internals"
severity: MEDIUM
domain: ui
lens: code-smells
labels:
  - "inappropriate-intimacy"
  - "cdc_os_ui"
---

## Summary
In `components/cdc_os_ui/src/AppUi.cpp`, the `updatePowerStatusIcons()` function directly accesses hardware controller instances via `hal::getWifiControllerInstance()` and `hal::getBluetoothControllerInstance()`, creating tight coupling between the UI and HAL.

## Impact
**Tight coupling**: UI knows too much about HAL structure.

**Testing difficulty**: Cannot test UI without full HAL mock.

**Violation of dependency inversion**: UI depends on concrete HAL implementations.

## Evidence
`components/cdc_os_ui/src/Appui.cpp:218-233`:
```cpp
void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;

    // ... power status handling ...

    // WiFi status - directly accessing HAL!
    auto* wifi = hal::getWifiControllerInstance();
    bool wifiConnected = wifi && wifi->isConnected();
    if (wifiConnected != s_lastWifiConnected) {
        if (wifiConnected) s_lockScreen->addStatusIcon(StatusIcon::WIFI);
        else s_lockScreen->removeStatusIcon(StatusIcon::WIFI);
        s_lastWifiConnected = wifiConnected;
    }

    // BLE status - directly accessing HAL!
    auto* ble = hal::getBluetoothControllerInstance();
    bool bleEnabled = ble && ble->isEnabled();
    if (bleEnabled != s_lastBleEnabled) {
        if (bleEnabled) s_lockScreen->addStatusIcon(StatusIcon::BLE);
        else s_lockScreen->removeStatusIcon(StatusIcon::BLE);
        s_lastBleEnabled = bleEnabled;
    }
}
```

The UI layer is directly calling `hal::getWifiControllerInstance()` and `hal::getBluetoothControllerInstance()`.

## Recommended Fix
1. **Create a status provider interface**:
```cpp
class IStatusProvider {
public:
    virtual bool isWifiConnected() const = 0;
    virtual bool isBleEnabled() const = 0;
    virtual bool isUsbConnected() const = 0;
    virtual bool isCharging() const = 0;
    virtual uint8_t getBatteryPercent() const = 0;
};
```

2. **Implement in HAL layer**:
```cpp
class HalStatusProvider : public IStatusProvider {
public:
    bool isWifiConnected() const override {
        auto* wifi = hal::getWifiControllerInstance();
        return wifi && wifi->isConnected();
    }
    // ... other methods
};
```

3. **Inject into UI**:
```cpp
void ui_init(const UiDeps& deps, IStatusProvider* statusProvider) {
    // ...
}
```

**Estimated effort**: ~1 hour to create interface and update dependencies.

## References
- Refactoring.com: "Inappropriate Intimacy" - https://refactoring.com/catalog/moveMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
