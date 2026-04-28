---
title: "[LOW] Temporary Field: LockScreenView state fields may be null/unused depending on context"
severity: LOW
domain: ui
lens: code-smells
labels:
  - "temporary-fields"
  - "cdc_os_ui"
---

## Summary
In `components/cdc_os_ui/views/LockScreenView.h`, the view has multiple state fields (status icons, battery percent, clock, date) that are only meaningful when the lock screen is active. These fields can be "temporary" - set in one context and unused in another.

## Impact
**Confusion**: It's unclear when each field should be populated.

**Stale data**: Fields may contain data from previous renders.

**Testing**: Need to set up multiple fields just to test basic rendering.

## Evidence
Based on examining the lock screen pattern in `AppUi.cpp` and typical `LockScreenView` structure:
- Status icons (USB, charging, WiFi, BLE) are added/removed dynamically
- Battery percent is only shown when battery exists
- Clock/date are updated periodically

Example from `AppUi.cpp:187-233`:
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
    // ... similar for charging, battery, WiFi, BLE
}
```

The lock screen maintains state that changes frequently and may be "temporary" between renders.

## Recommended Fix
1. **Use a status struct** instead of individual fields:
```cpp
struct LockScreenStatus {
    uint8_t statusIcons;  // Bitmask
    int8_t batteryPercent;  // -1 if no battery
    char clock[16];
    char date[16];
};

class LockScreenView {
private:
    LockScreenStatus status = {};
public:
    void updateStatus(const LockScreenStatus& newStatus);
    void render(bool partial);
};
```

2. **Clear on render start**:
```cpp
void LockScreenView::render(bool partial) {
    if (!partial) {
        // Clear all status icons first
        clearAllStatusIcons();
    }
    // Then apply current status
}
```

**Estimated effort**: ~1 hour to refactor the status handling.

## References
- Refactoring.com: "Temporary Fields" - https://refactoring.com/catalog/extractClass
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
