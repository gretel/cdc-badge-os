---
title: "[HIGH] AppUiInternal.h creates tight coupling between UI modules"
severity: HIGH
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `AppUiInternal.h` file acts as a "god header" that tightly couples all UI modules together. It declares shared state, helper functions, and cross-file dependencies in one place, causing `AppUi.cpp`, `WifiMenuUi.cpp`, `BluetoothMenuUi.cpp`, and `ExpertMenuUi.cpp` to all depend on this central file. This creates a fragile monolithic structure where changes ripple across multiple files.

**Evidence:**
- File: `components/cdc_os_ui/src/AppUiInternal.h`
- Imported by: `AppUi.cpp`, `WifiMenuUi.cpp`, `BluetoothMenuUi.cpp`, `ExpertMenuUi.cpp`
- Contains: Global constants, shared drawing helpers, cross-file menu rebuild functions, feature sub-file entry points

## Impact
1. **Circular dependency risk**: All UI files depend on `AppUiInternal.h`, which itself references types from all of them
2. **Change impact**: Modifying any function signature in `AppUiInternal.h` potentially breaks all 4 dependent files
3. **Testing difficulty**: Cannot test `BluetoothMenuUi` independently without pulling in `WifiMenuUi` and `ExpertMenuUi` dependencies
4. **Scalability problem**: As new menu files are added, they all depend on this same central header, increasing coupling
5. **Hidden state**: Static variables and functions scattered across files but coordinated through this header

## Evidence
**AppUiInternal.h** structure (lines 1-60):
```cpp
// Shared Constants
static constexpr uint32_t TOAST_DURATION_SHORT_MS = 1000;
static constexpr uint32_t TOAST_DURATION_MEDIUM_MS = 1500;
static constexpr uint32_t TOAST_DURATION_LONG_MS = 2500;

// Shared Drawing Helpers
void drawSignalBars(Gdey029T94* gfx, int x, int y, int8_t rssi, bool inverted);

// Cross-file Menu Rebuild Functions
void rebuildMainMenu();
void rebuildToolsMenu();

// Feature Sub-file Entry Points
void showWifiMainMenu();
void rebuildWifiMainMenu();
void showBluetoothMenu();
void rebuildBluetoothMenu();
void showExpertMenu();
void showModulesView();
void onModuleErrorEvent(const core::Event& evt);
```

**Dependencies (all 4 files include AppUiInternal.h):**
```
components/cdc_os_ui/src/AppUi.cpp: #include "AppUiInternal.h"
components/cdc_os_ui/src/BluetoothMenuUi.cpp: #include "AppUiInternal.h"
components/cdc_os_ui/src/WifiMenuUi.cpp: #include "AppUiInternal.h"
components/cdc_os_ui/src/ExpertMenuUi.cpp: #include "AppUiInternal.h"
```

## Recommended Fix
**Option A: Extract feature-specific handlers (Recommended)**
1. Create `WifiHandlers.h` with only WiFi-related declarations (10 minutes)
2. Create `BluetoothHandlers.h` with only BLE-related declarations (10 minutes)
3. Create `ExpertHandlers.h` with only Expert menu declarations (10 minutes)
4. Move `rebuildMainMenu()` and `rebuildToolsMenu()` to a new `MenuBuilder.h` (10 minutes)
5. Update each file to include only its specific handler header + `MenuBuilder.h` (10 minutes)
6. Remove `AppUiInternal.h` or reduce to only truly shared constants (5 minutes)

**Option B: Use EventBus for decoupling (Alternative)**
1. Define event types for menu rebuilds (5 minutes)
2. Each feature publishes its own events (10 minutes)
3. AppUi subscribes to all events (10 minutes)
4. Remove cross-file function calls (10 minutes)

**Total estimated time: ~45-60 minutes**

## References
- Module architecture: `CLAUDE.md` - "Modules must be isolated - no cross-references from core to modules"
- EventBus pattern already exists in `cdc_core/EventBus.h`
- Similar to `mod_fido2` pattern where each feature file has its own header
