---
title: "[MEDIUM] PIN change wizard lacks validation for matching PINs until final step"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `PinChangeView.cpp:180-225`, the wizard flow for changing a PIN has three steps (current PIN, new PIN, confirm PIN), but the "new PIN" step has no validation until the confirm step. If the user enters a very short PIN or one that doesn't match, they only find out after pressing 'Y' on the confirm step.

## Impact
- Poor user flow: user might enter a new PIN, then have to re-enter it from scratch after a mismatch error
- No guidance on PIN requirements during entry (minimum length, format rules)
- Error recovery requires re-entering the entire new PIN, not just correcting the last entry

## Evidence
**File: `components/cdc_os_ui/src/views/PinChangeView.cpp:180-195`**
```cpp
case Step::NEW_PIN: {
    if (length_ < minLength_) {
        showMessage(tr(StringId::PIN_TOO_SHORT));
        clearBuffer();
        return;
    }
    // Move to confirm step - NO validation of PIN strength or format
    step_ = Step::CONFIRM_PIN;
    clearBuffer();
    message_ = nullptr;
    // ...
}
```

**File: `components/cdc_os_ui/src/views/PinChangeView.cpp:197-222`**
```cpp
case Step::CONFIRM_PIN: {
    // Only here is the match checked
    if (strcmp(newPin_, confirmPin_) != 0) {
        showMessage(tr(StringId::PIN_MISMATCH));
        // Go back to new PIN step - user must re-enter entire new PIN
        step_ = Step::NEW_PIN;
        memset(newPin_, 0, sizeof(newPin_));
        clearBuffer();
        LOG_W(TAG, "PIN mismatch, re-enter new PIN");
        return;
    }
    // ...
}
```

## Recommended Fix
1. Add a character counter showing progress (e.g., "3/4 digits") during new PIN entry
2. Show a hint in the footer about minimum length requirements
3. Consider preserving the new PIN on mismatch and only requiring the user to re-verify (or provide an "edit" option)
4. Extend error message duration to at least 3000ms for readability

## References
- Nielsen Norman Group: [Multi-Step Form Design](https://www.nngroup.com/articles/multi-step-forms/)
- Material Design: [Text Fields - Validation](https://material.io/components/text-fields#validation)
