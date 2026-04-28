---
title: "[MEDIUM] Password entry wizard has no cancel/exit mechanism"
severity: MEDIUM
domain: UI/UX - Wizard Flow
lens: flow-dead-ends
labels:
  - "audit:ux-antipatterns/flow-dead-ends"
---

## Summary
The Password module's add/edit-entry wizard (in `components/mod_password/src/PasswordModule.cpp`) is a multi-step flow with 6 steps (title, username, password, URL, TOTP slot, notes). Once the user enters the wizard, there is **no way to cancel or exit** the flow except by completing all steps.

**Location:** `components/mod_password/src/PasswordModule.cpp`, lines 540-660

The wizard consists of:
1. `wizardStart()` / `wizardEdit()` → T9InputView for title (line 556-580)
2. `onWizardTitle()` → T9InputView for username (line 593)
3. `onWizardUsername()` → T9InputView for password (line 603)
4. `onWizardPassword()` → T9InputView for URL (line 613)
5. `onWizardUrl()` → T9InputView for TOTP slot (line 625)
6. `onWizardTotp()` → T9InputView for notes (line 639)
7. `onWizardNotes()` → Finalizes entry (line 653)

## Impact
Users who accidentally enter the Password wizard (e.g., from the main menu by pressing Y on "New Entry") are **trapped in the forward-only flow**. They cannot:
- Go back to the previous step
- Cancel the wizard entirely
- Exit to the main list view

This is especially problematic because:
1. The wizard has 6 sequential steps with no back navigation
2. The T9InputView's 'N' key only performs backspace, not cancel
3. The only way out is to complete all 6 steps or enter invalid data

## Evidence
**Wizard flow code (lines 540-660):**

```cpp
static void pushT9WizardStep(const char* title, const char* initialText,
                              uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}

static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_wizard.entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;
    s_wizard.editMode = false;
    s_wizard.editSlot = 0;

    pushT9WizardStep(mstr(STR_TITLE), nullptr, PasswordStore::TITLE_LEN, onWizardTitle);
}

static void onWizardTitle(const char* text) {
    strncpy(s_wizard.entry.title, text ? text : "", sizeof(s_wizard.entry.title) - 1);
    pushT9WizardStep(mstr(STR_USERNAME), s_wizard.entry.username, PasswordStore::USERNAME_LEN, onWizardUsername);
}

// ... similar pattern for onWizardUsername, onWizardPassword, onWizardUrl, onWizardTotp ...

static void onWizardNotes(const char* text) {
    strncpy(s_wizard.entry.notes, text ? text : "", sizeof(s_wizard.entry.notes) - 1);
    wizardFinish();
}
```

**T9InputView 'N' key handling (from `components/cdc_views/src/T9InputView.cpp`, line 218):**

```cpp
case 'N':  // Backspace
    backspace();
    return InputResult::CONSUMED;
```

The 'N' key only performs backspace, it doesn't provide a way to cancel the entire wizard.

**T9InputView long-press 'N' handling (from `components/cdc_views/src/T9InputView.cpp`, lines 238-248):**

```cpp
InputResult T9InputView::onLongPress(char key) {
    if (key == 'N') {
        // Clear all text
        text_[0] = '\0';
        len_ = 0;
        lastKey_ = 0;
        cursorActive_ = false;
        dirty_ = true;
        return InputResult::CONSUMED;
    }
    // ... rest of existing code
}
```

When long-press 'N' clears all text, there's no callback to notify the wizard. The wizard must track state externally or rely on the user to press 'N' again when empty (which doesn't exist for T9InputView).

## Recommended Fix
Add a cancel mechanism to the Password wizard. Since this is the same pattern as GPG and TOTP wizards, the same solutions apply:

**Option 1: Long-press 'N' to cancel (recommended)**

Add a cancel callback to T9InputView (see issue #004 for the core fix), then use it in the Password wizard:

```cpp
// Add to T9InputView.h
using CancelCallback = void(*)(void);
void setOnCancel(CancelCallback callback) { onCancel_ = callback; }

// Update T9InputView.cpp onLongPress
InputResult T9InputView::onLongPress(char key) {
    if (key == 'N') {
        // Clear all text
        text_[0] = '\0';
        len_ = 0;
        lastKey_ = 0;
        cursorActive_ = false;
        dirty_ = true;
        
        // Call cancel callback if present
        if (onCancel_) {
            ViewStack::instance().pop();
            onCancel_();
            return InputResult::CONSUMED;
        }
        return InputResult::CONSUMED;
    }
    // ... rest of existing code
}

// Then in PasswordModule.cpp
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

Add '3' key to show a cancel confirmation in T9InputView:

```cpp
// In T9InputView::onKey
case '3': // Context menu
    ui::showConfirm("Cancel wizard?", 
        [](void*) { 
            // Return REQUEST_POP to pop view
            return InputResult::REQUEST_POP;
        },
        nullptr, ui::ConfirmView::Icon::QUESTION);
    return InputResult::CONSUMED;
```

**Option 3: Update footer hint to show cancel option**

```cpp
const char* T9InputView::getFooterHint() const {
    return "[Y] OK  [3] Cancel  [N] Backspace";
}
```

## References
- **User Flow Dead Ends:** Multi-step flows without back or exit (audit focus)
- **Wizard and Multi-Step Flows Without Back or Exit:** "Wizard flows with no cancel, close, or exit mechanism once the user has entered the first step"
- **Related issues:**
  - #001-gpg-wizard-no-cancel (same pattern in GPG module)
  - #002-totp-wizard-no-cancel (same pattern in TOTP module)
  - #004-t9input-no-cancel-callback (core component fix)
- **ViewStack API:** `components/cdc_ui/include/cdc_ui/ViewStack.h` - provides `pop()` for navigation
