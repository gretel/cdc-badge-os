---
title: "[LOW] T9InputView placeholder support is optional and not consistently used"
severity: LOW
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
`T9InputView.cpp:290-295` implements a placeholder feature that shows text when the input is empty, but this is optional and must be explicitly set via `setPlaceholder()`. In the password module wizard (`PasswordModule.cpp:590-650`), placeholders are not used, so users see an empty text box with no guidance on what to enter.

## Impact
- Users may not understand what field they are filling in a multi-step wizard
- Empty inputs provide no contextual hint
- Inconsistent UX: some fields have placeholders, others don't

## Evidence
**File: `components/cdc_views/src/T9InputView.cpp:290-295`**
```cpp
if (len_ == 0 && placeholder_) {
    // Show placeholder when empty
    gfx->setTextColor(EPD_DARKGREY);
    gfx->print(placeholder_);
    gfx->setTextColor(EPD_BLACK);
}
```

**File: `components/mod_password/src/PasswordModule.cpp:590-650`**
The wizard steps are pushed without setting placeholders:
```cpp
pushT9WizardStep(mstr(STR_TITLE), nullptr, PasswordStore::TITLE_LEN, onWizardTitle);
pushT9WizardStep(mstr(STR_USERNAME), s_wizard.entry.username, PasswordStore::USERNAME_LEN, onWizardUsername);
// ... etc - no placeholders set
```

## Recommended Fix
1. Use the title text as an implicit placeholder when the field is empty (e.g., show "Title" in gray until user types)
2. Or, ensure all wizard steps call `setPlaceholder()` with a helpful hint
3. Consider making placeholder behavior automatic based on the title

## References
- Material Design: [Text Fields - Placeholder](https://material.io/components/text-fields#anatomy)
- WCAG 1.3.1: [Info and Relationships](https://www.w3.org/WAI/WCAG21/Understanding/info-and-relationships.html)
