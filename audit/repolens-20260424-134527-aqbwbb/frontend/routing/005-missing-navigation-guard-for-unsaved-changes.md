---
title: "[LOW] No Navigation Guard for Unsaved Changes"
severity: LOW
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
Forms and wizards (TOTP add/edit, Password add/edit, GPG key generation) don't warn users if they try to navigate away before saving changes. Pressing N (back) during a wizard simply pops the view, potentially losing all entered data.

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp` (wizard steps)
- `components/mod_password/src/PasswordModule.cpp` (wizard steps)
- `components/mod_gpg/src/GpgModule.cpp` (wizard steps)

## Impact
Data loss risk:
1. User fills in a multi-step wizard (e.g., TOTP account with name, secret, issuer)
2. User accidentally presses N (back) or long-press N
3. Wizard pops without warning, all entered data is lost
4. User must start over

## Evidence
From `TotpModule.cpp:448-462` (TotpCodeView onKey):
```cpp
ui::InputResult onKey(char key) override {
    if (key == 'N') {
        return ui::InputResult::REQUEST_POP;  // Just pops, no warning
    }
    if (key == '3') {
        wizardEdit(slot_);
        return ui::InputResult::CONSUMED;
    }
    // ...
}
```

From `ViewStack.cpp:175-180`:
```cpp
void ViewStack::dispatchKey(char key) {
    // ...
    if (result == InputResult::REQUEST_POP) {
        pop();  // No guard, just pops
    }
}
```

The wizard has 4-5 steps (name, secret, issuer, digits, algorithm, period). If user presses N at any step, they lose all progress.

## Recommended Fix
Add a simple "unsaved changes" guard:

**Option 1: Track dirty state in wizard**
```cpp
struct WizardState {
    char name[TotpStore::NAME_LEN + 1];
    // ... other fields
    bool hasChanges = false;  // Set to true when user enters data
};

static void onWizardName(const char* text) {
    strncpy(s_wizard.name, text ? text : "", sizeof(s_wizard.name) - 1);
    s_wizard.hasChanges = true;  // Mark as dirty
    pushT9WizardStep(mstr(STR_SECRET), s_wizard.secret, 64, onWizardSecret);
}

// In T9InputView onKey:
if (key == 'N' && wizard.hasChanges) {
    // Show confirm dialog
    showConfirm("Discard changes?", [](void*) {
        wizard.hasChanges = false;
        ViewStack::instance().pop();
    });
    return InputResult::CONSUMED;  // Don't pop yet
}
```

**Option 2: Use existing ConfirmView**
```cpp
void showUnsavedChangesConfirm() {
    showConfirm("Discard changes?",
                [](void*) { ViewStack::instance().pop(); },
                nullptr,  // Cancel does nothing
                ConfirmView::Icon::WARNING);
}
```

**Option 3: Add to ViewStack**
Add a guard callback that views can register:
```cpp
using NavigationGuard = bool(*)();  // Return true to allow navigation
void setNavigationGuard(NavigationGuard guard);
bool canNavigate();  // Check guard before pop
```

## References
- ConfirmView already exists: `components/cdc_views/include/cdc_views/ConfirmView.h`
- Consider adding this to IView interface as `bool hasUnsavedChanges()`
