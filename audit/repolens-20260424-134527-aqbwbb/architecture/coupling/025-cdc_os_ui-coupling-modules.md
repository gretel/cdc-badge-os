---
title: "[MEDIUM] cdc_os_ui creates coupling to modules through hardcoded references"
severity: MEDIUM
domain: architecture/coupling
lens: cdc_os_ui-coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
The `cdc_os_ui` component (OS-level UI) includes and references specific modules like GPG. This creates reverse coupling: core UI depends on modules, but modules should depend on core UI. This breaks the module isolation pattern.

**Evidence:**
- `components/mod_gpg/src/GpgModule.cpp` (line 14): Includes `cdc_os_ui/views/PinChangeView.h`
- `components/cdc_os_ui/src/AppUi.cpp` (line 14): Includes `cdc_os_ui/views/PinChangeView.h`
- GPG module uses `PinChangeView` which is in `cdc_os_ui`

## Impact
**Reverse dependency:** The architecture should be:
```
modules → cdc_os_ui → cdc_views → cdc_ui
```

But instead we have:
```
cdc_os_ui ← mod_gpg (GPG depends on cdc_os_ui views)
```

**Tight coupling:** If `PinChangeView` moves or changes, GPG module breaks.

**Module isolation violated:** Modules should be self-contained. GPG shouldn't depend on `cdc_os_ui`.

**Example:**
```cpp
// mod_gpg/src/GpgModule.cpp (line 14)
#include "cdc_os_ui/views/PinChangeView.h"

// GPG uses PinChangeView for PIN changes:
static void showPinChangeView() {
    auto* view = PinChangeView::getInstance();
    view->init("Change PIN", ...);
    ViewStack::instance().push(view);
}
```

## Evidence
**File: `components/mod_gpg/src/GpgModule.cpp` (lines 8-14)**
```cpp
#include "cdc_views/ListView.h"
#include "cdc_views/T9InputView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/QRCodeView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/ToastView.h"
#include "cdc_os_ui/views/PinChangeView.h"  // ← cdc_os_ui dependency
```

**File: `components/cdc_os_ui/src/AppUi.cpp` (lines 12-18)**
```cpp
#include "cdc_os_ui/AppUi.h"
#include "cdc_os_ui/views/LockScreenView.h"
#include "cdc_os_ui/views/PinChangeView.h"  // ← PinChangeView in cdc_os_ui
#include "cdc_os_ui/WifiHandlers.h"
#include "cdc_os_ui/SettingsHandlers.h"
#include "cdc_os_ui/SleepManager.h"
#include "cdc_os_ui/HardwareInfo.h"
```

**File: `components/cdc_os_ui/src/views/PinChangeView.cpp`**
```cpp
// PinChangeView is in cdc_os_ui but used by mod_gpg
// This creates reverse dependency
```

## Recommended Fix
**Option 1: Move PinChangeView to cdc_views**
PIN change is a generic UI component, not OS-specific. Move to `cdc_views`:
```
components/cdc_views/include/cdc_views/PinChangeView.h
components/cdc_views/src/PinChangeView.cpp
```

**Option 2: Create module-specific views**
GPG should have its own `GpgPinChangeView` in `mod_gpg`:
```
components/mod_gpg/include/mod_gpg/GpgPinChangeView.h
components/mod_gpg/src/GpgPinChangeView.cpp
```

**Option 3: Event-based decoupling**
GPG emits a "change PIN" event. `cdc_os_ui` listens and shows the view.

## References
- `components/mod_gpg/src/GpgModule.cpp` - GPG uses PinChangeView
- `components/cdc_os_ui/src/views/PinChangeView.h` - PinChangeView location
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - View navigation
- `components/cdc_views/` - Reusable views
