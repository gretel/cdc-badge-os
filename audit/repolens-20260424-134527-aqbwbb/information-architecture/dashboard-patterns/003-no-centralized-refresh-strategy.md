---
title: "[LOW] No centralized refresh strategy for dashboard status indicators"
severity: LOW
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

Status indicators on the lock screen dashboard (WiFi, BLE, USB, battery) are updated independently with no centralized refresh management. Found in:

- `components/cdc_os_ui/src/AppUi.cpp:739-746` - `ui_process()` function
- `components/cdc_os_ui/src/AppUi.cpp:192-246` - `updatePowerStatusIcons()`

**Current behavior:**
- `updatePowerStatusIcons()` runs every `ui_process()` loop (potentially hundreds of times per second)
- Each status check queries hardware independently (`isUsbConnected()`, `getChargeStatus()`, etc.)
- No throttling or batching of status updates
- No visibility into which status was checked when

## Impact

**Performance:**
- Redundant hardware queries on every loop iteration
- Status updates happen at different rates (clock = 1/min, icons = every loop)
- No coordination between related updates

**Maintainability:**
- Adding new status indicators requires modifying `ui_process()`
- No clear place to configure refresh intervals
- Hard to implement "pause refresh when tab not visible" (for future BLE serial)

## Evidence

**Current Implementation:**
```cpp
// components/cdc_os_ui/src/AppUi.cpp:739-746
void ui_process(uint32_t nowMs) {
    // Update status icons when on lock screen
    if (s_lockScreen && ViewStack::instance().current() == s_lockScreen) {
        updatePowerStatusIcons();  // Runs every loop, no throttling
    }

    // Update clock at minute change
    updateLockScreenClock();  // Throttled to 1/min

    // ... rest of processing
}
```

```cpp
// components/cdc_os_ui/src/AppUi.cpp:192-246
void updatePowerStatusIcons() {
    if (!s_lockScreen || !s_deps.power) return;

    bool usbConnected = s_deps.power->isUsbConnected();  // Hardware query
    bool charging = (s_deps.power->getChargeStatus() == ...);  // Hardware query
    bool batteryPresent = s_deps.power->isBatteryPresent();  // Hardware query

    // WiFi status
    auto* wifi = hal::getWifiControllerInstance();
    bool wifiConnected = wifi && wifi->isConnected();  // Hardware query

    // BLE status
    auto* ble = hal::getBluetoothControllerInstance();
    bool bleEnabled = ble && ble->isEnabled();  // Hardware query

    // Each updates independently if changed
    if (usbConnected != s_lastUsbConnected) { ... }
    if (charging != s_lastCharging) { ... }
    if (wifiConnected != s_lastWifiConnected) { ... }
    if (bleEnabled != s_lastBleEnabled) { ... }
}
```

**What's Missing:**
- No refresh interval configuration (e.g., `STATUS_REFRESH_MS = 1000`)
- No batching of status updates
- No throttle mechanism for high-frequency loops
- No "last refreshed" tracking per indicator

## Recommended Fix

Implement a simple centralized refresh strategy:

1. **Add refresh interval constant:**
   ```cpp
   static constexpr uint32_t STATUS_REFRESH_INTERVAL_MS = 2000;  // 2 seconds
   static uint32_t s_lastStatusRefreshMs = 0;
   ```

2. **Throttle status updates in `ui_process()`:**
   ```cpp
   void ui_process(uint32_t nowMs) {
       // Update status icons (throttled)
       if (s_lockScreen && ViewStack::instance().current() == s_lockScreen) {
           if (nowMs - s_lastStatusRefreshMs >= STATUS_REFRESH_INTERVAL_MS) {
               updatePowerStatusIcons();
               s_lastStatusRefreshMs = nowMs;
           }
       }
       // ... rest
   }
   ```

3. **Optional: Add per-indicator refresh tracking:**
   ```cpp
   struct StatusRefresh {
       uint32_t lastUsbMs = 0;
       uint32_t lastWifiMs = 0;
       uint32_t lastBleMs = 0;
       uint32_t lastBatteryMs = 0;
   };
   static StatusRefresh s_statusRefresh = {};
   ```

This is ~1 hour of work and makes the refresh strategy explicit and configurable.

## References

- Dashboard pattern: Status indicators should refresh at reasonable intervals
- Performance: Avoid redundant hardware queries in tight loops
- Pattern: Throttled updates are common in monitoring dashboards
