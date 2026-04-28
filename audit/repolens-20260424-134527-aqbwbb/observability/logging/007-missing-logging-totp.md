---
title: "[LOW] Missing logging for TOTP code generation failures"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The TOTP module (`components/mod_totp/src/TotpModule.cpp`) lacks logging for code generation failures and time validation issues in the `TotpCodeView` component.

**Missing log entries:**

1. **Code generation** (lines 499-528): `updateCode()` method doesn't log failures:
```cpp
void updateCode() {
    TotpStore& store = TotpStore::instance();
    timeValid_ = store.isTimeValid();
    if (!timeValid_) {
        strncpy(code_, "------", sizeof(code_) - 1);
        remaining_ = 0;
        period_ = 0;
        return;  // No LOG_W for invalid time
    }
    
    TotpAccount account = {};
    if (!store.readAccount(slot_, &account)) {
        timeValid_ = false;
        strncpy(code_, "------", sizeof(code_) - 1);
        remaining_ = 0;
        period_ = 0;
        return;  // No LOG_E for read failure
    }
    ...
    int8_t rem = store.generateCode(slot_, code_);
    if (rem >= 0) {
        remaining_ = static_cast<uint8_t>(rem);
    } else {
        timeValid_ = false;
        strncpy(code_, "------", sizeof(code_) - 1);
        remaining_ = 0;
        period_ = 0;
        // No LOG_E for generate failure
    }
}
```

2. **Keyboard typing success** (lines 449-458): Success logged to UI but not to system log

## Impact
- **Debug difficulty**: Hard to diagnose why TOTP codes aren't generating
- **Time sync issues**: No log trail for when time becomes invalid
- **Account issues**: Read failures from storage aren't logged

## Evidence
The `TotpCodeView::updateCode()` method handles multiple failure cases but only shows UI feedback:
```cpp
// Line 361: Shows toast but no log
if (!timeValid_) {
    ui::showToastError(mstr(STR_TIME_INVALID));
}
```

## Recommended Fix
Add logging to TOTP code generation:

1. **In `updateCode()` method (around line 502)**:
```cpp
void updateCode() {
    TotpStore& store = TotpStore::instance();
    timeValid_ = store.isTimeValid();
    if (!timeValid_) {
        LOG_W(TAG, "Time not valid for TOTP code generation");
        strncpy(code_, "------", sizeof(code_) - 1);
        ...
    }
    
    TotpAccount account = {};
    if (!store.readAccount(slot_, &account)) {
        LOG_E(TAG, "Failed to read TOTP account for slot %u", slot_);
        ...
    }
    ...
    int8_t rem = store.generateCode(slot_, code_);
    if (rem >= 0) {
        remaining_ = static_cast<uint8_t>(rem);
        LOG_D(TAG, "Generated TOTP code for slot %u, %ds remaining", slot_, remaining_);
    } else {
        LOG_E(TAG, "TOTP code generation failed for slot %u", slot_);
        ...
    }
}
```

2. **In keyboard typing handler**:
```cpp
if (key == 'Y') {
    auto* kb = core::getKeyboard();
    if (kb && kb->isConnected()) {
        if (timeValid_ && code_[0] != '-') {
            kb->typeString(code_);
            ui::showToastSuccess("Typed");
            LOG_D(TAG, "TOTP code typed via keyboard");
        }
    }
}
```

## References
- `components/mod_totp/src/TotpModule.cpp` - TOTP module implementation
- `components/mod_totp/include/mod_totp/TotpStore.h` - TOTP storage interface
