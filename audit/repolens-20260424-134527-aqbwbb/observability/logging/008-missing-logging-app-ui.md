---
title: "[MEDIUM] Missing logging in AppUi core UI flow"
severity: MEDIUM
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The `AppUi.cpp` component (`components/cdc_os_ui/src/AppUi.cpp`) has **zero logging statements** despite implementing critical UI flow logic including lock screen management, PIN verification, menu navigation, and inactivity timeouts. This makes debugging UI issues extremely difficult.

**Key functions without logging:**
- `ui_init()` - UI initialization (line ~200)
- `onUnlockRequested()` - Lock screen unlock flow (line ~300)
- `onPinVerify()` - PIN verification callback (line ~310)
- `onPinSuccess()` - PIN success handling (line ~320)
- `onMainMenuSelect()` - Main menu navigation (line ~350)
- `onToolsSelect()` - Tools menu navigation (line ~400)
- `onSettingsSelect()` - Settings menu navigation (line ~450)
- `onInactivityTimeout()` - Auto-lock trigger (line ~550)
- `updatePowerStatusIcons()` - Status icon updates (line ~192)

## Impact
- **Debug difficulty**: When UI flow gets stuck or behaves unexpectedly, there's no log trail to diagnose
- **User experience**: Hard to understand why lock screen appears/disappears
- **Maintenance**: New developers can't trace UI state transitions without adding ad-hoc print statements
- **Inconsistent with project patterns**: All other core components (ServiceRegistry, ModuleRegistry, EventBus) have logging

## Evidence
`AppUi.cpp` does NOT include `cdc_log.h`:
```cpp
// Line 11-38: Includes in AppUi.cpp
#include "AppUiInternal.h"
#include "cdc_os_ui/AppUi.h"
#include "cdc_os_ui/views/LockScreenView.h"
...
#include "serial_cmd/SerialCmd.h"
#include "nvs.h"
// NO cdc_log.h include!
```

Compare with `PinChangeView.cpp` which has proper logging:
```cpp
// Line 175-224: PIN change flow with logging
LOG_I(TAG, "Current PIN verified, entering new PIN");
LOG_I(TAG, "New PIN entered, confirming");
LOG_W(TAG, "PIN mismatch, re-enter new PIN");
LOG_I(TAG, "PIN changed successfully");
LOG_E(TAG, "Failed to set new PIN");
```

Key UI flow in `AppUi.cpp` (lines ~300-350) has no logging:
```cpp
static void onUnlockRequested() {
    if (!s_deps.keypad) return;
    s_ignoreKeyUntilRelease = true;
    // No log entry for unlock attempt!
}

static bool onPinVerify(const char* pin) {
    core::PinManager& pm = core::PinManager::instance();
    bool ok = pm.verifyBadgePin(pin);
    // Returns true/false but no log of verification!
    return ok;
}
```

## Recommended Fix
Add logging to key UI flow points in `AppUi.cpp`:

1. **Add include** (line 11):
```cpp
#include "cdc_log.h"
```

2. **Add TAG definition** (after includes):
```cpp
static const char* TAG = "AppUi";
```

3. **In `ui_init()`** (around line 200):
```cpp
void ui_init(const UiDeps& deps) {
    LOG_I(TAG, "Initializing UI...");
    s_deps = deps;
    ...
    LOG_I(TAG, "UI initialized (lock screen: %s)", s_deps.secureElement ? "enabled" : "disabled");
}
```

4. **In `onUnlockRequested()`** (around line 300):
```cpp
static void onUnlockRequested() {
    LOG_D(TAG, "Unlock requested");
    if (!s_deps.keypad) {
        LOG_W(TAG, "No keypad for unlock");
        return;
    }
    s_ignoreKeyUntilRelease = true;
}
```

5. **In `onPinVerify()`** (around line 310):
```cpp
static bool onPinVerify(const char* pin) {
    core::PinManager& pm = core::PinManager::instance();
    bool ok = pm.verifyBadgePin(pin);
    LOG_I(TAG, "PIN verify: %s", ok ? "OK" : "FAIL");
    return ok;
}
```

6. **In `onInactivityTimeout()`** (around line 550):
```cpp
static void onInactivityTimeout() {
    LOG_W(TAG, "Inactivity timeout, locking screen");
    ...
}
```

## References
- `components/cdc_os_ui/src/AppUi.cpp` - Main UI flow implementation
- `components/cdc_os_ui/src/views/PinChangeView.cpp` - Example of proper PIN flow logging
- `components/cdc_log/include/cdc_log.h` - Logging API
