---
title: "[LOW] TOTP slot input lacks format hint for optional numeric field"
severity: LOW
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `PasswordModule.cpp:635-650`, the TOTP slot input accepts a number (0-255) but doesn't clearly communicate that leaving it empty is valid. The label says "(optional)" but users may not realize they can skip this field entirely.

## Impact
- Users may enter "0" thinking it's required when they could leave it empty
- Confusion about whether a slot number is needed
- Extra cognitive load to understand optional vs. required fields

## Evidence
**File: `components/mod_password/src/PasswordModule.cpp:635-650`**
```cpp
static void onWizardTotp(const char* text) {
    if (!text || !text[0]) {
        s_wizard.entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;  // Empty is valid
    } else {
        int value = atoi(text);
        if (value < 0 || value > 255) {
            ui::showToastError(mstr(STR_INVALID_INPUT));
            pushT9WizardStep(mstr(STR_TOTP_SLOT), text, 3, onWizardTotp);
            return;
        }
        s_wizard.entry.totpSlot = static_cast<uint8_t>(value);
    }
    // ...
}
```

**File: `components/mod_password/src/PasswordModule.cpp:79`**
Label is "TOTP Slot (optional)" but this is in the title, not as a clear input hint.

## Recommended Fix
1. Add a clear footer hint like "Enter slot (0-255) or press N to skip"
2. Show a default value or placeholder like "(leave empty for none)"
3. Consider auto-advancing if the user presses Y with an empty field (confirming they want to skip)

## References
- Nielsen Norman Group: [Optional vs. Required Fields](https://www.nngroup.com/articles/optional-required/)
- Material Design: [Text Fields - Helper Text](https://material.io/components/text-fields#anatomy)
