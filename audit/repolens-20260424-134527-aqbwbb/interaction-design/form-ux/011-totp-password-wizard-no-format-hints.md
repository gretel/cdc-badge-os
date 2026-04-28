---
title: "[MEDIUM] TOTP/password wizard lacks input format hints and validates only on confirm"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `PasswordModule.cpp:596-660` and `TotpModule.cpp:580-700`, the multi-step wizards for entering passwords and TOTP accounts use T9 input with no hints about expected format, valid ranges, or required vs optional fields. Validation happens only when the user confirms each step, and error messages are shown as transient toasts.

## Impact
- Users entering TOTP secrets may not know if they should use Base32 format
- TOTP slot input accepts any digits but only validates the 0-255 range on confirm
- No visual indication of character limits (e.g., password length, URL length)
- Users may enter invalid data and lose their progress if they navigate away
- Error messages in toasts disappear quickly and may not be associated with the specific field

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
            pushT9WizardStep(mstr(STR_TOTP_SLOT), text, 3, onWizardTotp);
            return;
        }
        s_wizard.entry.totpSlot = static_cast<uint8_t>(value);
    }
    pushT9WizardStep(mstr(STR_NOTES), s_wizard.entry.notes, NOTES_INPUT_MAX, onWizardNotes);
}
```

**File: `components/mod_password/src/PasswordModule.cpp:547-552`**
```cpp
static void pushT9WizardStep(const char* title, const char* initialText,
                              uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}
```
The T9InputView shows a character count (`len_`/`maxLen_`) but does not indicate:
- Expected format (e.g., "Base32 for secrets", "Number 0-255 for TOTP slot")
- Whether the field is required or optional
- What happens if the user enters too many characters

**File: `components/cdc_views/src/T9InputView.cpp:295-310`**
The placeholder text is shown when empty, but there's no persistent hint about valid input format.

## Recommended Fix
1. Add format hints to wizard step titles (e.g., "TOTP Slot (0-255, optional)")
2. Show inline validation as user types for numeric ranges (e.g., highlight if >255)
3. Add persistent footer hints explaining expected format for each step
4. For TOTP secrets, show a hint that Base32 format is expected (A-Z, 2-5)
5. Consider adding a "clear all" shortcut for long text fields (already exists with long-press N, but not documented)

## References
- Nielsen Norman Group: [Form Design Best Practices](https://www.nngroup.com/articles/form-design-placeholders/)
- Material Design: [Text Fields - Helper text](https://material.io/components/text-fields#helper-text)
