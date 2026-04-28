---
title: "[LOW] WifiMenuUi views allocated with new but never freed"
severity: LOW
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/cdc_os_ui/src/WifiMenuUi.cpp`, four UI view objects are allocated with `new` but never freed for the application lifetime.

**Location**: `components/cdc_os_ui/src/WifiMenuUi.cpp:241,413,467,527` (allocations), no cleanup

```cpp
// Line 241: WiFi main menu
s_wifiMainMenu = new ListView();

// Line 413: WiFi scan view
s_wifiScanView = new ListView();

// Line 467: WiFi auth menu
s_wifiAuthMenu = new ListView();

// Line 527: WiFi IP menu
s_wifiIpMenu = new ListView();

// No destructor or cleanup method exists!
```

## Impact

- **Memory leak**: 4 ListView objects (~128-256 bytes total) persist for application lifetime.
- **Minor impact**: Small memory footprint, one-time allocation.
- **Consistency issue**: Follows same pattern as other modules that don't clean up.

### Context

WifiMenuUi provides WiFi configuration and scanning functionality. Views are created lazily on first use and persist until application shutdown.

## Evidence

**File**: `components/cdc_os_ui/src/WifiMenuUi.cpp`

**Line 238-245** (WiFi main menu):
```cpp
if (!s_wifiMainMenu) {
    s_wifiMainMenu = new ListView();  // Allocated once, never freed
    s_wifiMainMenu->setOnSelect(onWifiMainSelect);
    // ...
}
```

**Line 410-417** (WiFi scan view):
```cpp
if (!s_wifiScanView) {
    s_wifiScanView = new ListView();  // Allocated once, never freed
    s_wifiScanView->setOnSelect(onWifiScanSelect);
    // ...
}
```

**Line 464-471** (WiFi auth menu):
```cpp
if (!s_wifiAuthMenu) {
    s_wifiAuthMenu = new ListView();  // Allocated once, never freed
    s_wifiAuthMenu->setOnSelect(onWifiAuthSelect);
    // ...
}
```

**Line 524-531** (WiFi IP menu):
```cpp
if (!s_wifiIpMenu) {
    s_wifiIpMenu = new ListView();  // Allocated once, never freed
    s_wifiIpMenu->setOnSelect(onWifiIpSelect);
    // ...
}
```

**No cleanup method**: No destructor or `cleanup()` method to free views.

## Recommended Fix

### Option 1: Add static instances (recommended)

Change to static instances like other modules:

```cpp
// Change declarations
static ListView s_wifiMainMenu;
static ListView s_wifiScanView;
static ListView s_wifiAuthMenu;
static ListView s_wifiIpMenu;

// Change allocations (remove "new")
if (s_wifiMainMenu) {  // Already exists
    // Update if needed
} else {
    s_wifiMainMenu.setOnSelect(onWifiMainSelect);
    // ...
}
```

### Option 2: Add cleanup method

```cpp
// In WifiMenuUi.h
void cleanup();

// In WifiMenuUi.cpp
void WifiMenuUi::cleanup() {
    delete s_wifiMainMenu;
    s_wifiMainMenu = nullptr;
    delete s_wifiScanView;
    s_wifiScanView = nullptr;
    delete s_wifiAuthMenu;
    s_wifiAuthMenu = nullptr;
    delete s_wifiIpMenu;
    s_wifiIpMenu = nullptr;
}
```

## References

- Related to: Issues #001 (TOTP), #004 (Password), #005 (NVS Edit), #006 (AppUi), #007 (ExpertMenu), #008 (BluetoothMenu) memory leak patterns
- Consistency: Follow same pattern as other modules

</content>