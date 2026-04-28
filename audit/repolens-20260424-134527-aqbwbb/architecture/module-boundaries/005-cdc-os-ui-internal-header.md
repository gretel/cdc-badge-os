---
title: "[LOW] cdc_os_ui: Internal header in src/ included by multiple files"
severity: LOW
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `cdc_os_ui` module has an internal header `AppUiInternal.h` in the `src/` directory that is included by multiple source files. While this is a common pattern, the header is not clearly documented as internal and could be confused for part of the public API:

- **`AppUiInternal.h`** - Located at `components/cdc_os_ui/src/AppUiInternal.h`
- Included by: `AppUi.cpp`, `BluetoothMenuUi.cpp`, `WifiMenuUi.cpp`, `ExpertMenuUi.cpp`
- Contains cross-file menu rebuild functions and shared constants

## Impact

- **Potential Confusion**: Developers might think `AppUiInternal.h` is part of the public API
- **Tight Coupling**: All files depending on this header create implicit coupling between UI sub-modules
- **Limited Encapsulation**: Internal functions are exposed across the entire `src/` directory

## Evidence

**Internal header structure:**
```cpp
// From components/cdc_os_ui/src/AppUiInternal.h:
#pragma once

#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_views/ListView.h"
#include "cdc_views/ToastView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/T9InputView.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/EventBus.h"

namespace cdc::ui {
    // Shared Constants
    static constexpr uint32_t TOAST_DURATION_SHORT_MS = 1000;
    
    // Shared Drawing Helpers
    void drawSignalBars(Gdey029T94* gfx, int x, int y, int8_t rssi, bool inverted);
    
    // Cross-file Menu Rebuild Functions
    void rebuildMainMenu();
    void rebuildToolsMenu();
    
    // Feature Sub-file Entry Points
    void showWifiMainMenu();
    void showBluetoothMenu();
    void showExpertMenu();
    // ...
}
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
#include "AppUiInternal.h"
```

## Recommended Fix

1. **Add clear internal marker**:
   ```cpp
   // components/cdc_os_ui/src/AppUiInternal.h
   #pragma once
   /**
    * @file AppUiInternal.h
    * @brief Internal header - NOT part of public API
    * 
    * This header is only used by cdc_os_ui source files.
    * Do not include from outside this module.
    */
   ```

2. **Consider refactoring**:
   - Extract shared constants to a dedicated `AppUiConstants.h` (still internal)
   - Create a `AppUiPrivate.h` for cross-file declarations
   - Use forward declarations where possible to reduce coupling

3. **Add to CMake documentation**:
   ```cmake
   # cdc_os_ui includes:
   #   Public: include/cdc_os_ui/
   #   Internal: src/ (AppUiInternal.h, etc.)
   ```

4. **Document in module header**:
   - Add note to `AppUi.h` listing internal files

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules must be: Removable, Isolated, Self-registering"
- IView interface: `components/cdc_ui/include/cdc_ui/IView.h`
