---
title: "[MEDIUM] cdc_os_ui component mixes menu UI, settings handlers, and feature-specific logic"
severity: MEDIUM
domain: cdc_os_ui
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
  - "component:cdc_os_ui"
---

## Summary
`components/cdc_os_ui/` component contains 9 source files with mixed concerns:
1. **AppUi.cpp** (782 lines) - Core UI, menu building, lock flow
2. **WifiMenuUi.cpp** (715 lines) - WiFi menu UI + settings handlers mixed
3. **BluetoothMenuUi.cpp** (313 lines) - BLE menu UI
4. **ExpertMenuUi.cpp** (303 lines) - Expert menu + module management
5. **SettingsHandlers.cpp** (282 lines) - Settings callbacks + badge text wizard
6. **WifiHandlers.cpp** (273 lines) - WiFi settings + IP parsing + SNTP sync
7. **SleepManager.cpp** (270 lines) - Sleep control + inactivity tracking
8. **HardwareInfo.cpp** (156 lines) - Hardware info display
9. **LockScreenView.cpp** (705 lines) - Lock screen view + status icon logic

Key coupling issues:
- `SettingsHandlers.cpp` depends on `LockScreenView.h` directly
- `WifiHandlers.cpp` mixes IP parsing with WiFi controller integration
- `WifiMenuUi.cpp` has 715 lines combining UI rendering and settings logic

## Impact
**Compilation coupling**: Changes to settings handlers force recompilation of all menu files.

**Testing complexity**: Testing WiFi menu requires mocking settings handlers.

**Feature isolation**: WiFi feature cannot be disabled without touching multiple files.

**Code duplication**: Similar menu patterns exist in WiFi, Bluetooth, Expert menus without shared abstraction.

## Evidence
File structure (`components/cdc_os_ui/src/`):
```
AppUi.cpp           (782 lines) - Core UI + menu building + lock flow
WifiMenuUi.cpp      (715 lines) - WiFi menu UI + settings
BluetoothMenuUi.cpp (313 lines) - BLE menu UI
ExpertMenuUi.cpp    (303 lines) - Expert menu + modules
SettingsHandlers.cpp (282 lines) - Settings callbacks
WifiHandlers.cpp    (273 lines) - WiFi settings + IP parsing
SleepManager.cpp    (270 lines) - Sleep control
HardwareInfo.cpp    (156 lines) - Hardware info
LockScreenView.cpp  (705 lines) - Lock screen + icons
```

Coupling evidence in `SettingsHandlers.cpp`:
```cpp
#include "cdc_os_ui/views/LockScreenView.h"  // Direct dependency
static LockScreenView* s_lockScreen = nullptr;  // Stored reference
```

Coupling evidence in `WifiHandlers.cpp`:
```cpp
#include "cdc_hal/IWifiController.h"  // HAL dependency
#include "esp_sntp.h"  // SNTP sync logic mixed with settings
```

## Recommended Fix
**Restructure component by feature** (each 1 hour task):

1. **Extract `cdc_os_ui_settings` component**:
   - Move `SettingsHandlers.cpp` to new component
   - Move `WifiHandlers.cpp` to new component
   - Provide `settings::init()`, `settings::onBrightnessSave()`, etc.

2. **Extract `cdc_os_ui_menu` component**:
   - Create shared menu base class
   - Extract common menu patterns
   - `WifiMenuUi`, `BluetoothMenuUi`, `ExpertMenuUi` use base

3. **Create `cdc_os_ui_lock` component**:
   - Move `LockScreenView.cpp` to new component
   - Move `PinChangeView.cpp` to new component
   - Provide `lock::init()`, `lock::show()`

4. **Create `cdc_os_ui_sleep` component**:
   - Move `SleepManager.cpp` to new component
   - Provide `sleep::init()`, `sleep::setInterval()`

5. **Refactor `AppUi.cpp`**:
   - Compose sub-components
   - Remove direct settings/handlers logic

**Files to create**:
- `components/cdc_os_ui_settings/` (new component)
- `components/cdc_os_ui_menu/` (new component)
- `components/cdc_os_ui_lock/` (new component)
- `components/cdc_os_ui_sleep/` (new component)

**Migration steps**:
1. Create `cdc_os_ui_settings`, move SettingsHandlers and WifiHandlers
2. Create `cdc_os_ui_lock`, move LockScreenView and PinChangeView
3. Create `cdc_os_ui_sleep`, move SleepManager
4. Create `cdc_os_ui_menu`, extract common menu patterns
5. Update `AppUi.cpp` to compose sub-components
6. Update `main/CMakeLists.txt` to include new components

## References
- SRP: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Component architecture: https://www.oreilly.com/library/view/software-architecture-patterns/9781492090114/
- ESP32 component design: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guidelines.html
