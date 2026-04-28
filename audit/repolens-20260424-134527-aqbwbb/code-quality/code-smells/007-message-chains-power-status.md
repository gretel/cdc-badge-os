---
title: "[LOW] Message Chain: AppUi::updatePowerStatusIcons() chains multiple get calls"
severity: LOW
domain: ui
lens: code-smells
labels:
  - "message-chains"
  - "cdc_os_ui"
---

## Summary
In `components/cdc_os_ui/src/AppUi.cpp:187-233`, the `updatePowerStatusIcons()` function chains multiple method calls to retrieve hardware state, creating tight coupling to the hardware abstraction structure.

## Impact
**Tight coupling**: The UI layer knows too much about the HAL structure.

**Fragile**: If any getter method changes signature, multiple call sites break.

**Hard to test**: Requires mocking multiple hardware interfaces.

## Evidence
`components/cdc_os_ui/src/AppUi.cpp:187-233`:
```cpp
void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;

    bool usbConnected = s_deps.power->isUsbConnected();
    bool charging = (s_deps.power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ||
                     s_deps.power->getChargeStatus() == hal::ChargeStatus::PRE_CHARGE);
    bool batteryPresent = s_deps.power->isBatteryPresent();

    if (usbConnected != s_lastUsbConnected) {
        if (usbConnected) s_lockScreen->addStatusIcon(StatusIcon::USB);
        else s_lockScreen->removeStatusIcon(StatusIcon::USB);
        s_lastUsbConnected = usbConnected;
    }

    if (charging != s_lastCharging) {
        if (charging) s_lockScreen->addStatusIcon(StatusIcon::CHARGING);
        else s_lockScreen->removeStatusIcon(StatusIcon::CHARGING);
        s_lastCharging = charging;
    }

    if (batteryPresent != s_lastBatteryPresent) {
        s_lockScreen->setBatteryPercent(batteryPresent ? s_deps.power->getBatteryPercent() : -1);
        s_lastBatteryPresent = batteryPresent;
    } else if (batteryPresent) {
        s_lockScreen->setBatteryPercent(s_deps.power->getBatteryPercent());
    }

    // WiFi status
    auto* wifi = hal::getWifiControllerInstance();
    bool wifiConnected = wifi && wifi->isConnected();
    if (wifiConnected != s_lastWifiConnected) {
        if (wifiConnected) s_lockScreen->addStatusIcon(StatusIcon::WIFI);
        else s_lockScreen->removeStatusIcon(StatusIcon::WIFI);
        s_lastWifiConnected = wifiConnected;
    }

    // BLE status
    auto* ble = hal::getBluetoothControllerInstance();
    bool bleEnabled = ble && ble->isEnabled();
    if (bleEnabled != s_lastBleEnabled) {
        if (bleEnabled) s_lockScreen->addStatusIcon(StatusIcon::BLE);
        else s_lockScreen->removeStatusIcon(StatusIcon::BLE);
        s_lastBleEnabled = bleEnabled;
    }
}
```

Multiple chains like `s_deps.power->isUsbConnected()`, `s_deps.power->getChargeStatus()`, `hal::getWifiControllerInstance()->isConnected()`.

## Recommended Fix
1. **Create a status aggregator class**:
```cpp
class PowerStatus {
public:
    bool isUsbConnected() const;
    bool isCharging() const;
    bool isBatteryPresent() const;
    uint8_t getBatteryPercent() const;
};

class SystemStatus {
public:
    PowerStatus power;
    bool wifiConnected() const;
    bool bleEnabled() const;
};
```

2. **Update `updatePowerStatusIcons()` to use the aggregator**:
```cpp
void updatePowerStatusIcons() {
    SystemStatus status;
    if (!s_lockScreen) return;

    if (status.power.isUsbConnected() != s_lastUsbConnected) {
        // ...
    }
}
```

**Estimated effort**: ~1 hour to create status aggregator and update the function.

## References
- Refactoring.com: "Message Chains" - https://refactoring.com/catalog/extractClass
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
