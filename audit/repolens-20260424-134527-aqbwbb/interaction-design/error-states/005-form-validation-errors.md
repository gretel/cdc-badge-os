---
title: "[MEDIUM] Form validation errors not mapped to specific fields"
severity: MEDIUM
domain: interaction-design/error-states
lens: form-validation
labels:
  - "audit:interaction-design/error-states"
---

## Summary
Multi-step form wizards (TOTP, Password, GPG) show generic validation errors at the end rather than highlighting which specific field needs attention.

**Affected wizards:**
- TOTP account wizard (`components/mod_totp/src/TotpModule.cpp`)
- Password entry wizard (`components/mod_password/src/PasswordModule.cpp`)
- GPG key generation wizard (`components/mod_gpg/src/GpgModule.cpp`)

## Impact
**User confusion:** When validation fails, users must manually check each field to find the problem.

**Inefficient workflow:** Users can't quickly correct the issue and must restart the wizard.

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:870
static void wizardFinish() {
    if (strlen(s_wizard.name) == 0 || strlen(s_wizard.secret) == 0) {
        ui::showToastError(mstr(STR_INVALID_INPUT));  // Generic "Invalid input"
        // Which field? Name? Secret? Both?
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
        return;
    }
    // ...
}

// components/mod_password/src/PasswordModule.cpp
static void onWizardTotp(const char* text) {
    if (!text || !text[0]) {
        s_wizard.entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;
    } else {
        int value = atoi(text);
        if (value < 0 || value > 255) {
            ui::showToastError(mstr(STR_INVALID_INPUT));  // Which field?
            pushT9WizardStep(mstr(STR_TOTP_SLOT), text, 3, onWizardTotp);  // Good - returns to field
            return;
        }
        s_wizard.entry.totpSlot = static_cast<uint8_t>(value);
    }
    pushT9WizardStep(mstr(STR_NOTES), s_wizard.entry.notes, NOTES_INPUT_MAX, onWizardNotes);
}

// Password wizard - no validation at all until finish
static void onWizardTitle(const char* text) {
    strncpy(s_wizard.entry.title, text ? text : "", sizeof(s_wizard.entry.title) - 1);
    pushT9WizardStep(mstr(STR_USERNAME), s_wizard.entry.username, PasswordStore::USERNAME_LEN, onWizardUsername);
    // No validation - title could be empty, user finds out at end
}
```

**GPG wizard has no validation:**
```cpp
// components/mod_gpg/src/GpgModule.cpp
static void onWizardName(const char* text) {
    strncpy(s_wizard.name, text ? text : "", sizeof(s_wizard.name) - 1);
    s_t9Input.init(mstr(STR_EMAIL), nullptr, 63);
    s_t9Input.setOnSave(onWizardEmail);
    ui::ViewStack::instance().push(&s_t9Input);
    // No validation - name could be empty
}

// ... all the way to
static void onWizardCurve(uint16_t index, void*) {
    s_wizard.curve = (index == 0) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    // Only then does it check if name/email are valid
    char user_id[GPG_USER_ID_MAX] = {};
    size_t name_len = strnlen(s_wizard.name, sizeof(s_wizard.name) - 1);
    // ...
    gpg_set_pending_user_id(user_id);
    if (gpg_generate_key(s_wizard.curve)) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));  // Generic
    }
}
```

## Recommended Fix
**1. Add per-field validation with specific error messages:**

```cpp
// T9InputView enhancement - return validation status
struct ValidationResult {
    bool valid;
    const char* fieldLabel;
    const char* error;
};

// Add validation callback
void T9InputView::setValidator(std::function<ValidationResult(const char*)> validator);

// Updated wizard with field-level validation
static void onWizardTitle(const char* text) {
    if (!text || strlen(text) < 2) {
        ui::showToastError("Title must be at least 2 characters");
        pushT9WizardStep(mstr(STR_TITLE), text, PasswordStore::TITLE_LEN, onWizardTitle);  // Stay on field
        return;
    }
    strncpy(s_wizard.entry.title, text, sizeof(s_wizard.entry.title) - 1);
    pushT9WizardStep(mstr(STR_USERNAME), s_wizard.entry.username, PasswordStore::USERNAME_LEN, onWizardUsername);
}

static void onWizardName(const char* text) {
    if (!text || strlen(text) < 2) {
        ui::showToastError("Name must be at least 2 characters");
        pushT9WizardStep(mstr(STR_NAME), text, 63, onWizardName);  // Stay on field
        return;
    }
    strncpy(s_wizard.name, text, sizeof(s_wizard.name) - 1);
    pushT9WizardStep(mstr(STR_EMAIL), nullptr, 63, onWizardEmail);
}
```

**2. Use ConfirmView with field navigation for final validation:**

```cpp
static void wizardFinish() {
    // Validate each field and show specific errors
    if (strlen(s_wizard.name) == 0) {
        ui::showToastError("Account name is required");  // Specific
        // Navigate back to name field
        while (ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
        return;
    }
    if (strlen(s_wizard.secret) == 0) {
        ui::showToastError("Secret is required");  // Specific
        // Navigate back to secret field
        while (ui::ViewStack::instance().depth() > 2) {
            ui::ViewStack::instance().pop();
        }
        return;
    }
    // All fields validated, proceed
}
```

**3. Add inline validation hints:**

```cpp
// T9InputView shows validation hint in footer
const char* T9InputView::getFooterHint() const {
    if (!valid_) {
        return validationError_;  // "Min 2 chars required"
    }
    return tr(StringId::HINT_T9_INPUT);
}
```

**Estimated effort:** 1 hour for T9InputView validation API, 30 min per wizard implementation

## References
- [Nielsen Norman Group: Inline Validation](https://www.nngroup.com/articles/inline-validation/)
- [Material Design: Text Fields - Validation](https://material.io/design/components/text-fields.html#state)
