---
title: "[MEDIUM] Password wizard loses input data on validation error"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `PasswordModule.cpp:640-650`, when the TOTP slot validation fails (e.g., user enters "300" which is > 255), the error message is shown but the user is pushed back to the same TOTP input step with the invalid text, which is good. However, in other wizard steps like the PIN change view, invalid input is cleared entirely.

## Impact
- Inconsistent behavior across different forms in the same application
- Users may lose their input unexpectedly when navigating back or correcting errors
- No clear pattern for how validation errors should be handled

## Evidence
**File: `components/mod_password/src/PasswordModule.cpp:640-650`**
```cpp
static void onWizardTotp(const char* text) {
    if (!text || !text[0]) {
        s_wizard.entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;
    } else {
        int value = atoi(text);
        if (value < 0 || value > 255) {
            ui::showToastError(mstr(STR_INVALID_INPUT));
            pushT9WizardStep(mstr(STR_TOTP_SLOT), text, 3, onWizardTotp);  // Preserves text - GOOD
            return;
        }
        s_wizard.entry.totpSlot = static_cast<uint8_t>(value);
    }
    pushT9WizardStep(mstr(STR_NOTES), s_wizard.entry.notes, NOTES_INPUT_MAX, onWizardNotes);
}
```

**File: `components/cdc_os_ui/src/views/PinChangeView.cpp:203-212`**
```cpp
if (strcmp(newPin_, confirmPin_) != 0) {
    showMessage(tr(StringId::PIN_MISMATCH));
    // Go back to new PIN step
    step_ = Step::NEW_PIN;
    memset(newPin_, 0, sizeof(newPin_));  // Clears the new PIN - user must re-enter
    clearBuffer();
    LOG_W(TAG, "PIN mismatch, re-enter new PIN");
    return;
}
```

## Recommended Fix
1. Standardize error handling across all wizard forms
2. For the PIN change wizard, preserve the new PIN on mismatch and allow the user to re-type only the confirmation
3. Or provide an "Edit" option to go back and modify the previous entry instead of re-entering everything

## References
- Nielsen Norman Group: [Form Design Best Practices](https://www.nngroup.com/articles/form-design-placeholders/)
- Material Design: [Forms - Error States](https://material.io/components/text-fields#anatomy)
