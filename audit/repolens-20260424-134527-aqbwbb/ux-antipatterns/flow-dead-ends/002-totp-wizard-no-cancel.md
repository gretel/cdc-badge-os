---
title: "[MEDIUM] TOTP account wizard has no cancel/exit mechanism"
severity: MEDIUM
domain: UI/UX - Wizard Flow
lens: flow-dead-ends
labels:
  - "audit:ux-antipatterns/flow-dead-ends"
---

## Summary
The TOTP module's account-addition wizard (in `components/mod_totp/src/TotpModule.cpp`) is a multi-step flow with 5 steps (name, secret, issuer, digits, algorithm, period). Once the user enters the wizard, there is **no way to cancel or exit** the flow except by completing all steps or triggering a validation error.

**Location:** `components/mod_totp/src/TotpModule.cpp`, lines 744-914

The wizard consists of:
1. `wizardStart()` → T9InputView for account name (line 752)
2. `onWizardName()` → T9InputView for secret (line 785)
3. `onWizardSecret()` → T9InputView for issuer (line 794)
4. `onWizardIssuer()` → ListView for digits selection (line 808)
5. `onWizardDigits()` → ListView for algorithm selection (line 823)
6. `onWizardAlgo()` → ListView for period selection (line 838)
7. `onWizardPeriod()` → Finalizes account (line 866)

## Impact
Users who accidentally enter the TOTP wizard are **trapped in the forward-only flow**. They cannot:
- Go back to the previous step
- Cancel the wizard entirely
- Exit to the main list view

This is especially problematic because:
1. The wizard has 5-6 sequential steps with no back navigation
2. The T9InputView's 'N' key only performs backspace, not cancel
3. The ListView's 'N' key returns `REQUEST_POP` but this only pops one view, leaving the user in the middle of the wizard
4. The only way out is to complete all steps or enter invalid data (which shows an error but still doesn't clearly guide the user out)

## Evidence
**Wizard flow code (lines 744-914):**

```cpp
static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_wizard.digits = TotpStore::DEFAULT_DIGITS;
    s_wizard.algorithm = static_cast<uint8_t>(TotpAlgorithm::SHA1);
    s_wizard.period = TotpStore::DEFAULT_PERIOD;
    s_wizard.editMode = false;
    s_wizard.editSlot = 0;

    pushT9WizardStep(mstr(STR_ACCOUNT_NAME), nullptr, TotpStore::NAME_LEN, onWizardName);
}

static void onWizardName(const char* text) {
    strncpy(s_wizard.name, text ? text : "", sizeof(s_wizard.name) - 1);
    pushT9WizardStep(mstr(STR_SECRET), s_wizard.secret, 64, onWizardSecret);
}

// ... similar pattern for onWizardSecret, onWizardIssuer ...

static void pushT9WizardStep(const char* title, const char* initialText,
                              uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}
```

**ListView 'N' key handling (from `components/cdc_views/src/ListView.cpp`, line 159):**

```cpp
case 'N': // Back
    return InputResult::REQUEST_POP;
```

This only pops one view from the stack. If the user is on step 3 of 5, pressing 'N' once will return them to step 2, not to the main list.

**T9InputView 'N' key handling (from `components/cdc_views/src/T9InputView.cpp`, lines 215-219):**

```cpp
case 'N':  // Backspace
    backspace();
    return InputResult::CONSUMED;
```

The 'N' key only performs backspace, it doesn't provide a way to cancel the entire wizard.

## Recommended Fix
Add a cancel mechanism to the TOTP wizard. Since this is a similar issue to the GPG wizard, the same solutions apply:

**Option 1: Long-press 'N' to cancel (recommended)**
Modify `T9InputView::onLongPress()` to support cancel callback when at the start of input:

```cpp
InputResult T9InputView::onLongPress(char key) {
    if (key == 'N' && len_ == 0) {
        // Long-press N at start of input = cancel
        if (onCancel_) {
            ViewStack::instance().pop();
            onCancel_();
            return InputResult::CONSUMED;
        }
    }
    // ... rest of existing code (forceDigit for numbers)
}
```

Then add a cancel callback to the wizard steps:
```cpp
static void onWizardCancel() {
    // Pop remaining wizard steps back to list view
    while (ui::ViewStack::instance().current() != &s_listView &&
           ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
    ui::showToast(ui::tr(ui::StringId::CANCELLED), 1000);
}

static void pushT9WizardStep(const char* title, const char* initialText,
                              uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    s_t9Input.setOnCancel(onWizardCancel);  // Add cancel callback
    ui::ViewStack::instance().push(&s_t9Input);
}
```

**Option 2: Use '3' key for context menu cancel**
Add '3' key to show a cancel confirmation:
```cpp
case '3': // Context menu
    ui::showConfirm("Cancel wizard?", 
        [](void*) { 
            while (ui::ViewStack::instance().current() != &s_listView &&
                   ui::ViewStack::instance().depth() > 1) {
                ui::ViewStack::instance().pop();
            }
        },
        nullptr, ui::ConfirmView::Icon::QUESTION);
    return InputResult::CONSUMED;
```

**Option 3: Update footer hint to show cancel option**
```cpp
const char* getFooterHint() const override {
    return "[Y] Next [3] Cancel [N] Backspace";
}
```

## References
- **User Flow Dead Ends:** Multi-step flows without back or exit (audit focus)
- **Wizard and Multi-Step Flows Without Back or Exit:** "Wizard flows with no cancel, close, or exit mechanism once the user has entered the first step"
- **Related issue:** #001-gpg-wizard-no-cancel (same pattern in GPG module)
- **ViewStack API:** `components/cdc_ui/include/cdc_ui/ViewStack.h` - provides `pop()` for navigation
