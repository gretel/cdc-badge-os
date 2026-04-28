---
title: "[MEDIUM] Missing data freshness indicators on lock screen dashboard"
severity: MEDIUM
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

The lock screen (main dashboard) displays real-time status information (time, date, WiFi, BLE, USB, battery) but lacks visual indicators showing **when data was last updated**. This is found in:

- `components/cdc_os_ui/src/AppUi.cpp:192-246` - `updatePowerStatusIcons()`
- `components/cdc_os_ui/src/AppUi.cpp:249-266` - `updateLockScreenClock()`
- `components/cdc_os_ui/src/AppUi.cpp:740-748` - `ui_process()` tick function

The status icons update only when values **change** (diff-based), but there is no:
1. "Last updated" timestamp on the dashboard
2. Visual staleness indicator (e.g., dimmed icons, refresh needed marker)
3. Manual refresh affordance from the lock screen

## Impact

**User Experience Impact:**
- Users cannot verify if status data is current or stale
- WiFi/BLE connection status may appear outdated without indication
- No way to know if the system is actively polling or if updates are happening

**Maintenance Impact:**
- Debugging connectivity issues is harder without freshness info
- Users may assume device is working when data is actually stale

## Evidence

**Current Implementation:**

```cpp
// components/cdc_os_ui/src/AppUi.cpp:740-748
void ui_process(uint32_t nowMs) {
    // Update status icons when on lock screen
    if (s_lockScreen && ViewStack::instance().current() == s_lockScreen) {
        updatePowerStatusIcons();  // Updates only on change
    }

    // Update clock at minute change
    updateLockScreenClock();  // Updates only on minute change

    // ... input processing
}
```

```cpp
// components/cdc_os_ui/src/AppUi.cpp:192-246
void updatePowerStatusIcons() {
    bool usbConnected = s_deps.power->isUsbConnected();
    bool charging = (s_deps.power->getChargeStatus() == hal::ChargeStatus::FAST_CHARGE ...);
    
    if (usbConnected != s_lastUsbConnected) {  // Only updates on CHANGE
        if (usbConnected) s_lockScreen->addStatusIcon(StatusIcon::USB);
        else s_lockScreen->removeStatusIcon(StatusIcon::USB);
        s_lastUsbConnected = usbConnected;
    }
    // ... similar for other icons
}
```

**What's Missing:**
- No timestamp tracking when each status was last checked
- No "last updated" display on lock screen
- No manual refresh trigger from lock screen context menu

## Recommended Fix

Add a simple freshness indicator to the lock screen:

1. **Track last update time** - Add a `lastStatusUpdate_` timestamp field to `LockScreenView`
2. **Display freshness on context menu** - When user presses context key (e.g., '3'), show "Last updated: HH:MM"
3. **Optional: Add refresh action** - Allow manual status refresh from context menu

Implementation steps:
1. Add `uint32_t lastStatusUpdateMs_` to `LockScreenView` in `components/cdc_os_ui/include/cdc_os_ui/views/LockScreenView.h`
2. Update timestamp in `updatePowerStatusIcons()` after each status check
3. Add context menu handler in `LockScreenView::onKey()` for '3' key showing freshness info
4. Display format: "Status fresh" (< 30s) or "Last: HH:MM" (older)

This is ~1 hour of work and significantly improves user trust in displayed data.

## References

- Dashboard best practices: Users should always know when data was last refreshed
- Pattern: "Last updated" indicators are standard in monitoring dashboards (Grafana, Kibana, etc.)
- Similar pattern: Mobile status bars show refresh time for weather widgets
