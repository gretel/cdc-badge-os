---
title: "[MEDIUM] Confirm buttons lack loading state during async operations"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `T9InputView.cpp:215-225` and `PinEntryView.cpp:135-174`, when the user presses 'Y' to confirm, there is no visual indication that the operation is in progress. If the callback involves a slow operation (e.g., secure element access, NVS write), the user may think the button didn't register and press it again.

## Impact
- Potential double-submission of forms
- User confusion when there's a delay between confirm and feedback
- No feedback that the system is "working" on the request

## Evidence
**File: `components/cdc_views/src/T9InputView.cpp:215-225`**
```cpp
case 'Y':  // Confirm
    commitCharacter();
    if (onSave_) {
        // Pop ourselves FIRST, then call callback
        ViewStack::instance().pop();
        onSave_(text_);  // If this is slow, user has no feedback
    }
    return InputResult::CONSUMED;
```

**File: `components/cdc_views/src/PinEntryView.cpp:135-174`**
```cpp
void PinEntryView::verify() {
    // ...
    bool valid = onVerify_(buffer_);  // If this is slow, no loading indicator
    if (valid) {
        LOG_I(TAG, "PIN verified successfully");
        if (onSuccess_) {
            onSuccess_();
        }
    }
    // ...
}
```

## Recommended Fix
1. Add a visual "processing" indicator (e.g., "Verifying..." text, spinner, or status bar)
2. Disable the confirm key during processing (ignore additional 'Y' presses)
3. Show a Toast or InfoView with a loading message during async operations

## References
- Nielsen Norman Group: [System Status Visibility](https://www.nngroup.com/articles/visibility-of-system-status/)
- Material Design: [Loading States](https://m2.material.io/components/progress-indicators)
