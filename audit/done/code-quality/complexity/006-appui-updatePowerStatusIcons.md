---
title: "[MEDIUM] Complex conditional logic in updatePowerStatusIcons() function"
severity: MEDIUM
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `updatePowerStatusIcons()` function in `components/cdc_os_ui/src/AppUi.cpp` (lines 187-235) has complex conditional logic with multiple nested `if` statements checking various power and status conditions. The function has 5 independent status checks (USB, charging, battery, WiFi, BLE) each with their own conditional logic.

**Estimated Cyclomatic Complexity: ~12** (above threshold of 10)

## Impact

**Readability:**
- Multiple levels of nested conditionals make flow hard to follow
- Similar patterns repeated for each status type
- Hard to add new status icons without increasing complexity

**Maintenance:**
- Changes to one status may affect others due to shared state
- Testing requires mocking multiple hardware dependencies

## Evidence

**File:** `components/cdc_os_ui/src/AppUi.cpp:187-235`

**Code excerpt:**
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
        if (batteryPresent) s_lockScreen->setBatteryPercent(s_deps.power->getBatteryPercent());
        else s_lockScreen->setBatteryPercent(-1);
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

**Branching count:**
- Line 188: `if (!s_lockScreen || !s_deps.power)` (+1)
- Line 193: `if (usbConnected != s_lastUsbConnected)` (+1)
- Line 194: `if (usbConnected)` (+1)
- Line 198: `if (charging != s_lastCharging)` (+1)
- Line 199: `if (charging)` (+1)
- Line 203: `if (batteryPresent != s_lastBatteryPresent)` (+1)
- Line 204: `if (batteryPresent)` (+1)
- Line 208: `else if (batteryPresent)` (+1)
- Line 214: `if (wifiConnected != s_lastWifiConnected)` (+1)
- Line 215: `if (wifiConnected)` (+1)
- Line 221: `if (bleEnabled != s_lastBleEnabled)` (+1)
- Line 222: `if (bleEnabled)` (+1)

Total: ~12 independent paths

## Recommended Fix

**Extract status update logic into a generic helper:**

1. Create a helper function for single status updates:
```cpp
/**
 * \brief Updates a single status icon based on current state.
 * \param lockScreen Lock screen instance.
 * \param current Current state.
 * \param last Previous state (updated if changed).
 * \param icon Status icon to update.
 * \param valueFunc Optional function to get value when adding icon.
 */
static void updateStatusIcon(LockScreenView* lockScreen,
                             bool current, bool& last,
                             StatusIcon icon,
                             std::function<int()> valueFunc = nullptr) {
    if (current != last) {
        if (current) {
            if (valueFunc) {
                lockScreen->setBatteryPercent(valueFunc());
            } else {
                lockScreen->addStatusIcon(icon);
            }
        } else {
            if (valueFunc) {
                lockScreen->setBatteryPercent(-1);
            } else {
                lockScreen->removeStatusIcon(icon);
            }
        }
        last = current;
    } else if (current && valueFunc) {
        // Update value even if state unchanged
        lockScreen->setBatteryPercent(valueFunc());
    }
}
```

2. Simplified main function:
```cpp
void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;

    bool usbConnected = s_deps.power->isUsbConnected();
    bool charging = (s_deps.power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ||
                     s_deps.power->getChargeStatus() == hal::ChargeStatus::PRE_CHARGE);
    bool batteryPresent = s_deps.power->isBatteryPresent();

    updateStatusIcon(s_lockScreen, usbConnected, s_lastUsbConnected, StatusIcon::USB);
    updateStatusIcon(s_lockScreen, charging, s_lastCharging, StatusIcon::CHARGING);
    updateStatusIcon(s_lockScreen, batteryPresent, s_lastBatteryPresent, StatusIcon::BATTERY,
                     [power = s_deps.power]() { return power->getBatteryPercent(); });

    // WiFi status
    auto* wifi = hal::getWifiControllerInstance();
    bool wifiConnected = wifi && wifi->isConnected();
    updateStatusIcon(s_lockScreen, wifiConnected, s_lastWifiConnected, StatusIcon::WIFI);

    // BLE status
    auto* ble = hal::getBluetoothControllerInstance();
    bool bleEnabled = ble && ble->isEnabled();
    updateStatusIcon(s_lockScreen, bleEnabled, s_lastBleEnabled, StatusIcon::BLE);
}
```

**Alternative (using array of config structs):**
```cpp
struct StatusConfig {
    bool current;
    bool& last;
    StatusIcon icon;
    std::function<int()> valueFunc;
};

static void updateAllStatusIcons(const std::initializer_list<StatusConfig>& configs) {
    for (auto& cfg : configs) {
        if (cfg.current != cfg.last) {
            if (cfg.current) {
                if (cfg.valueFunc) {
                    s_lockScreen->setBatteryPercent(cfg.valueFunc());
                } else {
                    s_lockScreen->addStatusIcon(cfg.icon);
                }
            } else {
                if (cfg.valueFunc) {
                    s_lockScreen->setBatteryPercent(-1);
                } else {
                    s_lockScreen->removeStatusIcon(cfg.icon);
                }
            }
            cfg.last = cfg.current;
        } else if (cfg.current && cfg.valueFunc) {
            s_lockScreen->setBatteryPercent(cfg.valueFunc());
        }
    }
}

void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;

    bool usbConnected = s_deps.power->isUsbConnected();
    bool charging = (s_deps.power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ||
                     s_deps.power->getChargeStatus() == hal::ChargeStatus::PRE_CHARGE);
    bool batteryPresent = s_deps.power->isBatteryPresent();

    updateAllStatusIcons({
        {usbConnected, s_lastUsbConnected, StatusIcon::USB, nullptr},
        {charging, s_lastCharging, StatusIcon::CHARGING, nullptr},
        {batteryPresent, s_lastBatteryPresent, StatusIcon::BATTERY,
         [power = s_deps.power]() { return power->getBatteryPercent(); }},
    });

    auto* wifi = hal::getWifiControllerInstance();
    updateAllStatusIcons({
        {(wifi && wifi->isConnected()), s_lastWifiConnected, StatusIcon::WIFI, nullptr},
    });

    auto* ble = hal::getBluetoothControllerInstance();
    updateAllStatusIcons({
        {(ble && ble->isEnabled()), s_lastBleEnabled, StatusIcon::BLE, nullptr},
    });
}
```

**Expected result:**
- Main function reduced to ~20 lines
- Helper function has complexity ~4
- Easier to add new status icons
- Better testability with isolated helper

## References

- [Extract Function refactoring](https://refactoring.com/catalog/extractFunction.html)
- [Replace Conditional with Strategy](https://refactoring.com/catalog/replaceConditionalsWithPolymorphism.html)
