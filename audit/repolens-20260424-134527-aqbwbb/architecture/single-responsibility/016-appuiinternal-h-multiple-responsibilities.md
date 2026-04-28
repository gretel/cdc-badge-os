---
title: "[MEDIUM] AppUiInternal.h mixes drawing helpers, menu rebuilds, and feature entry points"
severity: MEDIUM
domain: cdc_os_ui
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
  - "component:cdc_os_ui"
---

## Summary
`components/cdc_os_ui/src/AppUiInternal.h` (66 lines) serves as a "catch-all" internal header mixing three unrelated concerns:
1. **Drawing helpers** - `drawSignalBars()` for WiFi/BLE RSSI visualization
2. **Menu rebuild functions** - `rebuildMainMenu()`, `rebuildToolsMenu()`, `rebuildBluetoothMenu()`
3. **Feature entry points** - `showWifiMainMenu()`, `showBluetoothMenu()`, `showExpertMenu()`, `onModuleErrorEvent()`

Key code evidence:
- Lines 28-32: Toast duration constants
- Lines 38-43: Drawing helper `drawSignalBars()`
- Lines 47-50: Menu rebuild functions
- Lines 54-62: Feature entry points for WiFi, Bluetooth, Expert, Modules

## Impact
**Tight coupling**: Drawing logic is exposed to menu files, creating circular dependencies between UI rendering and menu management.

**Compilation overhead**: Changes to menu functions trigger recompilation of all files including `AppUiInternal.h`.

**Discoverability**: Drawing helpers are buried in a menu-related header, making them hard to find.

**Scalability**: As new features are added, this header becomes a dumping ground for "internal" functions.

## Evidence
File: `components/cdc_os_ui/src/AppUiInternal.h`

Lines 38-43 (Drawing helper):
```cpp
/**
 * Draw RSSI signal bars (used by WiFi and BLE scan views)
 */
void drawSignalBars(Gdey029T94* gfx, int x, int y, int8_t rssi, bool inverted);
```

Lines 47-50 (Menu rebuilds):
```cpp
// Called by feature files when they need to trigger menu rebuilds
void rebuildMainMenu();
void rebuildToolsMenu();
```

Lines 54-62 (Feature entry points):
```cpp
// WiFi (WifiMenuUi.cpp)
void showWifiMainMenu();
void rebuildWifiMainMenu();

// Bluetooth (BluetoothMenuUi.cpp)
void showBluetoothMenu();
void rebuildBluetoothMenu();

// Expert & Modules (ExpertMenuUi.cpp)
void showExpertMenu();
void showModulesView();
void onModuleErrorEvent(const core::Event& evt);
```

## Recommended Fix
**Split into focused headers** (each 1 hour task):

1. **Create `AppUiDrawing.h`**: Only drawing helpers:
   - `drawSignalBars()`
   - Later: other UI drawing utilities

2. **Create `AppUiMenus.h`**: Only menu functions:
   - `rebuildMainMenu()`, `rebuildToolsMenu()`
   - `rebuildWifiMainMenu()`, `rebuildBluetoothMenu()`

3. **Create `AppUiFeatures.h`**: Only feature entry points:
   - `showWifiMainMenu()`, `showBluetoothMenu()`
   - `showExpertMenu()`, `showModulesView()`
   - `onModuleErrorEvent()`

4. **Update `AppUiInternal.h`**: Keep only truly internal state:
   - Shared constants (toast durations)
   - Internal state declarations

**Files to create**:
- `components/cdc_os_ui/src/AppUiDrawing.h`
- `components/cdc_os_ui/src/AppUiMenus.h`
- `components/cdc_os_ui/src/AppUiFeatures.h`

**Migration steps**:
1. Create `AppUiDrawing.h`, move `drawSignalBars()`
2. Create `AppUiMenus.h`, move menu rebuilds
3. Create `AppUiFeatures.h`, move feature entry points
4. Update all `.cpp` files to include correct headers

## References
- SRP: https://en.wikipedia.org/wiki/Single-responsibility_principle
- C++ header organization: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-headers
- Embedded UI patterns: https://www.embedded.com/design/prototyping-and-development/4023983/
