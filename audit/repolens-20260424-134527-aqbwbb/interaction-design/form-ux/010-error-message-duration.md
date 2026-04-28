---
title: "[MEDIUM] Error message display duration too short for readability"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
Error messages in `PinEntryView.cpp:165-174` and `PinChangeView.cpp:230-249` use a 1500ms (1.5 second) display duration. This is too brief for users to read and understand the error, especially on an E-Paper display which may have slower refresh rates.

## Impact
- Users may miss error messages entirely if they're not looking at the screen
- Short duration doesn't allow time for the user to comprehend the error
- Users may repeatedly trigger the same error without understanding why

## Evidence
**File: `components/cdc_views/src/PinEntryView.cpp:165-174`**
```cpp
if (pm.isBadgeBlocked()) {
    lockedOut_ = true;
    if (showMessages_) {
        showMessage(tr(StringId::LOCKED_OUT), MessageIcon::ERROR, 3000);  // 3s for lockout
    }
} else {
    if (showMessages_) {
        showMessage(tr(StringId::WRONG_PIN), MessageIcon::ERROR, 1500);  // 1.5s for wrong PIN
    }
}
```

**File: `components/cdc_os_ui/src/views/PinChangeView.cpp:230-249`**
```cpp
void PinChangeView::onTick(uint32_t nowMs) {
    if (messageShownMs_ > 0 && message_ != nullptr) {
        if (nowMs - messageShownMs_ >= MESSAGE_DISPLAY_MS) {  // MESSAGE_DISPLAY_MS = 2000ms
            message_ = nullptr;
            messageShownMs_ = 0;
            // ...
        }
    }
}
```

Inconsistent durations (1500ms, 2000ms, 3000ms) across different views.

## Recommended Fix
1. Standardize error message duration to at least 3000ms (3 seconds)
2. Consider making duration dynamic based on message length (longer text = more time to read)
3. Add a way for users to dismiss messages early (e.g., press any key)

## References
- Nielsen Norman Group: [Reading on Screens](https://www.nngroup.com/articles/reading-speed/)
- WCAG 3.3.1: [Error Identification](https://www.w3.org/WAI/WCAG21/Understanding/error-identification.html)
