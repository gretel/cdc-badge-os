---
title: "[MEDIUM] TOTP module shows error instead of cached code when time is invalid"
severity: MEDIUM
domain: graceful-degradation
lens: error-handling
labels:
  - "totp"
  - "time-sync"
  - "graceful-degradation"
---

## Summary
In `components/mod_totp/src/TotpModule.cpp`, the `TotpCodeView::updateCode()` method (line 487-521) checks time validity and immediately displays "------" when time is not set. The view doesn't offer any fallback behavior like showing the last valid code, allowing manual time override, or indicating how to sync time.

**File**: `components/mod_totp/src/TotpModule.cpp`  
**Lines**: 487-521 (specifically lines 491-496, 502-508)

## Impact
- **User Experience**: When the device loses time (after deep sleep or power cycle), TOTP codes become unusable without clear guidance on how to fix it.
- **Graceful Degradation**: The module could show cached codes with a "time may be stale" warning, or provide a quick time-sync option.
- **Recoverability**: No path to recover without navigating away and finding time settings elsewhere.

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:487-521
void TotpCodeView::updateCode() {
    TotpStore& store = TotpStore::instance();
    timeValid_ = store.isTimeValid();
    if (!timeValid_) {
        strncpy(code_, "------", sizeof(code_) - 1);  // Just shows dashes
        remaining_ = 0;
        period_ = 0;
        return;  // No fallback, no recovery hint
    }

    TotpAccount account = {};
    if (!store.readAccount(slot_, &account)) {
        timeValid_ = false;
        strncpy(code_, "------", sizeof(code_) - 1);
        remaining_ = 0;
        period_ = 0;
        return;
    }
    // ...
}

// In onEnter/onResume (lines 342-351, 356-362):
void TotpCodeView::onEnter(void* context) override {
    (void)context;
    updateCode();
    if (!timeValid_) {
        ui::showToastError(mstr(STR_TIME_INVALID));  // Shows error but no recovery path
    }
    dirty_ = true;
}
```

When time is invalid, the user sees:
1. "------" as the code
2. A toast error "Time not set"
3. No actionable guidance

## Recommended Fix
Add graceful degradation options:

**Option 1: Show cached code with warning**
```cpp
void TotpCodeView::updateCode() {
    TotpStore& store = TotpStore::instance();
    timeValid_ = store.isTimeValid();
    if (!timeValid_) {
        // Try to show last known good code with timestamp warning
        if (lastValidCode_[0]) {
            strncpy(code_, lastValidCode_, sizeof(code_) - 1);
            // Mark view as showing stale data
            staleData_ = true;
        } else {
            strncpy(code_, "------", sizeof(code_) - 1);
            staleData_ = false;
        }
        remaining_ = 0;
        period_ = 0;
        return;
    }
    // ... normal operation, save code for fallback
    strncpy(lastValidCode_, code_, sizeof(lastValidCode_) - 1);
}

const char* TotpCodeView::getFooterHint() const override {
    if (!timeValid_) {
        return "Time invalid: [S] Sync  [3] Edit";
    }
    // ... existing
}
```

**Option 2: Add time-sync shortcut**
```cpp
ui::InputResult TotpCodeView::onKey(char key) override {
    if (key == 'S' && !timeValid_) {
        // Open time sync view or NTP sync trigger
        ui::ViewStack::instance().push(&s_timeSyncView);
        return ui::InputResult::CONSUMED;
    }
    // ... existing
}

const char* TotpCodeView::getFooterHint() const override {
    auto* kb = core::getKeyboard();
    if (!timeValid_) {
        return "[S] Sync time  [3] Edit  [N] Back";
    }
    if (kb && kb->isConnected()) {
        return mstr(STR_HINT_TYPE);
    }
    return mstr(STR_HINT_EDIT);
}
```

**Option 3: Show time status with action**
```cpp
void TotpCodeView::render(bool partial) override {
    // ... existing rendering ...
    
    if (!timeValid_) {
        // Show time status with actionable hint
        gfx->setTextSize(1);
        gfx->setCursor(8, 60);
        gfx->print(mstr(STR_TIME_INVALID));
        gfx->setCursor(8, 75);
        gfx->print("Press [S] to sync time");  // Actionable hint
        clearDirty();
        return;
    }
    // ...
}
```

## References
- TOTP RFC 6238: Time-based One-Time Password algorithm
- Graceful degradation: Show useful information even when ideal conditions aren't met
- Error recovery: Provide clear paths to fix common problems
