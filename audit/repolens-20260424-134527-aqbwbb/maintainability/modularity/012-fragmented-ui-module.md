---
title: "[MEDIUM] cdc_os_ui module is fragmented into many loosely-connected files"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `cdc_os_ui` component is split into 9+ source files (`AppUi.cpp`, `WifiMenuUi.cpp`, `BluetoothMenuUi.cpp`, `ExpertMenuUi.cpp`, `HardwareInfo.cpp`, `SettingsHandlers.cpp`, `SleepManager.cpp`, `WifiHandlers.cpp`) with unclear boundaries and dependencies. Most files depend on `AppUiInternal.h` which creates a central coupling point.

**Evidence:**
- `components/cdc_os_ui/src/` contains:
  - `AppUi.cpp` (782 lines) - Core UI setup
  - `AppUiInternal.h` - Shared state and functions
  - `BluetoothMenuUi.cpp` - BLE menu
  - `ExpertMenuUi.cpp` - Expert menu
  - `HardwareInfo.cpp` - Hardware info display
  - `SettingsHandlers.cpp` - Settings logic
  - `SleepManager.cpp` - Sleep management
  - `WifiHandlers.cpp` - WiFi handlers
  - `WifiMenuUi.cpp` (2340 lines) - WiFi menu
- All files depend on `AppUiInternal.h`

## Impact
1. **Unclear module boundaries**: What belongs in `AppUi.cpp` vs `WifiMenuUi.cpp` vs `WifiHandlers.cpp`?
2. **Central coupling**: `AppUiInternal.h` becomes a "god header" that all files depend on
3. **Testing difficulty**: Cannot test `WifiMenuUi` without loading `AppUiInternal` dependencies
4. **Discoverability**: New developers struggle to find where to add new features
5. **Refactoring risk**: Changes to `AppUiInternal.h` affect all 9+ files

## Evidence
**File dependencies (all include AppUiInternal.h):**
```
AppUi.cpp: #include "AppUiInternal.h"
BluetoothMenuUi.cpp: #include "AppUiInternal.h"
WifiMenuUi.cpp: #include "AppUiInternal.h"
ExpertMenuUi.cpp: #include "AppUiInternal.h"
```

**AppUiInternal.h exports:**
```cpp
// Constants
TOAST_DURATION_SHORT_MS, TOAST_DURATION_MEDIUM_MS, TOAST_DURATION_LONG_MS

// Drawing helpers
drawSignalBars()

// Cross-file menu rebuild functions
rebuildMainMenu(), rebuildToolsMenu()

// Feature entry points
showWifiMainMenu(), showBluetoothMenu(), showExpertMenu(), showModulesView()
```

## Recommended Fix
**Phase 1: Clarify module boundaries (45 minutes)**
1. Create `components/cdc_os_ui/include/cdc_os_ui/` directory structure (5 minutes)
2. Move `AppUiInternal.h` functions to specific headers:
   - `MenuBuilder.h` - Menu rebuild functions
   - `WifiUi.h` - WiFi UI functions
   - `BluetoothUi.h` - Bluetooth UI functions
   - `ExpertUi.h` - Expert menu functions
3. Move constants to `UiConstants.h`
4. Move `drawSignalBars()` to `RenderHelpers.h` (already exists in cdc_views!)

**Phase 2: Add namespace wrappers (30 minutes)**
1. Wrap all files in `namespace cdc::ui { }`
2. Update includes to use `cdc_os_ui/` prefix

**Phase 3: Extract sub-modules (future work)**
- `mod_wifi` - WiFi-specific logic
- `mod_ble` - Bluetooth-specific logic

**Total estimated time for Phase 1+2: ~75 minutes** (split into 2-3 issues)

## References
- Module architecture: `CLAUDE.md` - "Modules must be isolated - no cross-references"
- Similar pattern to `mod_fido2` with better file organization
- Single Responsibility Principle: Each file should have one clear purpose
