---
title: "[MEDIUM] GPG key-generation wizard has no cancel/exit mechanism"
severity: MEDIUM
domain: UI/UX - Wizard Flow
lens: flow-dead-ends
labels:
  - "audit:ux-antipatterns/flow-dead-ends"
---

## Summary
The GPG module's key-generation wizard (in `components/mod_gpg/src/GpgModule.cpp`) is a multi-step flow that guides users through entering a name, email, and selecting a curve. However, once the user enters the wizard, there is **no way to cancel or exit** the flow except by completing all steps.

**Location:** `components/mod_gpg/src/GpgModule.cpp`, lines 413-497

The wizard consists of:
1. `wizardStart()` → T9InputView for name (line 413)
2. `onWizardName()` → T9InputView for email (line 433)
3. `onWizardEmail()` → ListView for curve selection (line 441)
4. `onWizardCurve()` → Finalizes key generation (line 455)

## Impact
Users who accidentally enter the GPG key-generation wizard (e.g., from the main menu) are **trapped in the forward-only flow**. They cannot:
- Go back to the previous step
- Cancel the wizard entirely
- Exit to the main menu

This is especially problematic because:
1. The wizard has 3 sequential steps with no back navigation
2. The T9InputView's 'N' key only performs backspace, not cancel
3. The ListView for curve selection only has 'Y' (select) and 'N' (back), but 'N' on the last step doesn't propagate properly to pop the entire wizard

## Evidence
**Wizard flow code (lines 413-497):**

```cpp
static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_t9Input.init(mstr(STR_NAME), nullptr, 63);
    s_t9Input.setOnSave(onWizardName);
    ui::ViewStack::instance().push(&s_t9Input);
}

static void onWizardName(const char* text) {
    strncpy(s_wizard.name, text ? text : "", sizeof(s_wizard.name) - 1);
    s_t9Input.init(mstr(STR_EMAIL), nullptr, 63);
    s_t9Input.setOnSave(onWizardEmail);
    ui::ViewStack::instance().push(&s_t9Input);
}

static void onWizardEmail(const char* text) {
    strncpy(s_wizard.issuer, text ? text : "", sizeof(s_wizard.issuer) - 1);
    static ui::ListItem curveItems[] = {
        { nullptr, 0, false, nullptr },
        { nullptr, 0, false, nullptr },
    };
    curveItems[0].label = mstr(STR_CURVE_ED25519);
    curveItems[1].label = mstr(STR_CURVE_P256);
    s_curveView.init(mstr(STR_CURVE), curveItems, 2);
    s_curveView.setOnSelect(onWizardCurve);
    ui::ViewStack::instance().push(&s_curveView);
}
```

**T9InputView onKey handler (from `components/cdc_views/src/T9InputView.cpp`, lines 206-225):**

```cpp
InputResult T9InputView::onKey(char key) {
    switch (key) {
        case 'Y':  // Confirm
            commitCharacter();
            if (onSave_) {
                ViewStack::instance().pop();
                onSave_(text_);
            }
            return InputResult::CONSUMED;

        case 'N':  // Backspace
            backspace();
            return InputResult::CONSUMED;

        default:
            if (key >= '0' && key <= '9') {
                processKey();
                return InputResult::CONSUMED;
            }
            return InputResult::IGNORED;
    }
}
```

The 'N' key only performs backspace when text exists, but doesn't provide a way to cancel the entire wizard. The `InputResult::REQUEST_POP` is never returned from the T9InputView unless there's no text to backspace AND no cancel callback.

## Recommended Fix
Add a cancel mechanism to the GPG wizard:

**Option 1: Long-press 'N' to cancel**
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
    // ... rest of existing code
}
```

Then add a cancel callback to the wizard:
```cpp
static void onWizardCancel() {
    // Pop remaining wizard steps
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
    ui::showToast(ui::tr(ui::StringId::CANCELLED), 1000);
}
```

**Option 2: Add '3' key context menu for cancel**
Similar to LockScreenView's context menu, add '3' key to show a cancel option:
```cpp
case '3': // Context menu
    ui::showConfirm("Cancel wizard?", 
        [](void*) { 
            // Pop all wizard steps
            while (ui::ViewStack::instance().depth() > 1) {
                ui::ViewStack::instance().pop();
            }
        },
        nullptr, ui::ConfirmView::Icon::QUESTION);
    return InputResult::CONSUMED;
```

**Option 3: Show footer hint with cancel option**
Update the footer hint for wizard steps to show the cancel option clearly:
```cpp
const char* getFooterHint() const override {
    return "[Y] Next [3] Cancel [N] Backspace";
}
```

## References
- **User Flow Dead Ends:** Multi-step flows without back or exit (audit focus)
- **Wizard and Multi-Step Flows Without Back or Exit:** "Wizard flows with no cancel, close, or exit mechanism once the user has entered the first step"
- **ViewStack API:** `components/cdc_ui/include/cdc_ui/ViewStack.h` - provides `pop()` for navigation
- **Similar pattern:** `PinChangeView` (lines 275-290) shows how to handle cancel from first step vs. back to previous step
