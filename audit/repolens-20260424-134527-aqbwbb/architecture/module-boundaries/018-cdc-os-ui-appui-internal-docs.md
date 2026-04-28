---
title: "[LOW] cdc_os_ui: Internal AppUiInternal.h lacks clear documentation marker"
severity: LOW
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `cdc_os_ui` module has an internal header `AppUiInternal.h` with basic documentation, but lacks a clear "NOT part of public API" marker that would make the internal status unambiguous:

- **`components/cdc_os_ui/src/AppUiInternal.h`** - Internal header shared between UI sub-files
- Currently has a brief comment but no strong internal marker

## Evidence

**Current header documentation:**
```cpp
// components/cdc_os_ui/src/AppUiInternal.h
#pragma once

/**
 * Internal header shared between AppUi sub-files.
 * Not part of the public API - only used by AppUi*.cpp and *MenuUi.cpp files.
 */
```

**Files including it:**
```cpp
// From components/cdc_os_ui/src/AppUi.cpp:
#include "AppUiInternal.h"

// From components/cdc_os_ui/src/BluetoothMenuUi.cpp:
#include "AppUiInternal.h"

// From components/cdc_os_ui/src/WifiMenuUi.cpp:
#include "AppUiInternal.h"

// From components/cdc_os_ui/src/ExpertMenuUi.cpp:
#include "AppuiInternal.h"
```

**Contents exposed:**
```cpp
// Shared Constants
static constexpr uint32_t TOAST_DURATION_SHORT_MS = 1000;
static constexpr uint32_t TOAST_DURATION_MEDIUM_MS = 1500;

// Shared Drawing Helpers
void drawSignalBars(Gdey029T94* gfx, int x, int y, int8_t rssi, bool inverted);

// Cross-file Menu Rebuild Functions
void rebuildMainMenu();
void rebuildToolsMenu();

// Feature Sub-file Entry Points
void showWifiMainMenu();
void showBluetoothMenu();
void showExpertMenu();
```

## Impact

- **Potential Confusion**: Developer may not notice the "internal" comment
- **Weak Enforcement**: No strong marker to indicate internal status
- **Documentation Debt**: Should follow consistent pattern with other modules

## Recommended Fix

1. **Add stronger internal marker**:
   ```cpp
   // components/cdc_os_ui/src/AppUiInternal.h
   #pragma once
   /**
    * @file AppUiInternal.h
    * @brief Internal header - NOT part of public API
    *
    * This header is only used by cdc_os_ui source files.
    * Do not include from outside this module.
    *
    * Used by:
    * - AppUi.cpp
    * - BluetoothMenuUi.cpp
    * - WifiMenuUi.cpp
    * - ExpertMenuUi.cpp
    */
   ```

2. **Add Doxygen group for internal API**:
   ```cpp
   /**
    * @defgroup cdc-os-ui-internal Internal API
    * @brief cdc_os_ui internal implementation
    *
    * These symbols are only for use within cdc_os_ui.
    * External modules should not depend on them.
    */
   ```

3. **Document in module header**:
   - Add note to `AppUi.h` listing internal files

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules must be: Removable, Isolated, Self-registering"
- IView interface: `components/cdc_ui/include/cdc_ui/IView.h`

(End of file - total 128 lines)
