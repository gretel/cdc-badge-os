---
title: "[MEDIUM] WifiMenuUi.cpp combines menu building, wizard flow, scan management, and IP validation"
severity: MEDIUM
domain: cdc_os_ui
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/cdc_os_ui/src/WifiMenuUi.cpp` (715 lines) handles multiple distinct responsibilities:
1. **Menu building** - `rebuildWifiMenu()`, `rebuildAuthMenu()`, `rebuildIpMenu()`
2. **Wizard flow** - Password input, IP configuration, setup completion
3. **Scan management** - Network scanning, result sorting, peer list display
4. **IP validation** - `isValidIpOctet()`, `isValidIpAddress()`, `parseIpAddress()`
5. **View initialization** - All WiFi views created and managed in one place
6. **Callback routing** - All menu selection callbacks registered and handled

## Impact
- **High coupling**: Menu structure, scan logic, and IP validation all in same file
- **Complexity**: 715 lines with mixed concerns makes navigation hard
- **Testing difficulty**: Cannot test IP validation without full UI initialization
- **Code reuse**: IP validation logic could be shared with other network modules

## Evidence
File: `components/cdc_os_ui/src/WifiMenuUi.cpp`
- Lines 22-59: Enum definitions (menu indices, auth modes, IP modes)
- Lines 61-99: Static state (views, buffers, scan results, 20+ variables)
- Lines 101-300: Menu building and callbacks
- Lines 300-500: Scan management (scan, sort, display)
- Lines 500-715: Wizard flow (password, IP input, validation)

Key pattern showing mixed concerns:
```cpp
// Menu building
static void rebuildWifiMenu() {
    s_wifiMainItems[0].label = "Connect";
    s_wifiMainItems[0].onSelect = onWifiMainSelect;
    // ...
}

// Scan management
static void sortWifiScanResults() {
    // Bubble sort by RSSI
    for (int i = 0; i < s_wifiScanCount - 1; i++) {
        for (int j = 0; j < s_wifiScanCount - i - 1; j++) {
            if (s_wifiScanResults[j].rssi < s_wifiScanResults[j+1].rssi) {
                // Swap
            }
        }
    }
}

// IP validation
static bool isValidIpOctet(const char* str) {
    if (!str || !*str) return false;
    int val = atoi(str);
    return val >= 0 && val <= 255;
}

// Wizard flow
static void onWifiPasswordEntered(const char* password) {
    // Store password, continue wizard
    strncpy(wifiConfig.password, password, ...);
    wifiShowIpModeMenu();
}
```

## Recommended Fix
Split into focused modules:
1. **WifiMenu** - Menu building in `components/cdc_os_ui/src/WifiMenu.cpp`
2. **WifiScanner** - Scan management in `components/cdc_os_ui/src/WifiScanner.cpp`
3. **WifiWizard** - Wizard flow in `components/cdc_os_ui/src/WifiWizard.cpp`
4. **IpHelpers** - IP validation in `components/cdc_hal/src/IpHelpers.cpp`

Each module should:
- Have its own header file
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/cdc_os_ui/src/
  WifiMenuUi.cpp  // Main orchestration, delegates to sub-modules
  WifiMenu.cpp    // Menu building
  WifiScanner.cpp // Scan management
  WifiWizard.cpp  // Wizard flow
components/cdc_hal/src/
  IpHelpers.cpp   // IP validation utilities
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Separation of Concerns: https://en.wikipedia.org/wiki/Separation_of_concerns
