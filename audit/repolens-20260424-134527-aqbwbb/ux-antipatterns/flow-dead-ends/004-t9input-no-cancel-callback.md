---
title: "[MEDIUM] T9InputView lacks cancel callback for wizard flows"
severity: MEDIUM
domain: UI/UX - Input Components
lens: flow-dead-ends
labels:
  - "audit:ux-antipatterns/flow-dead-ends"
---

## Summary
The `T9InputView` component (`components/cdc_views/src/T9InputView.cpp`) lacks a dedicated cancel callback mechanism. While it supports a save callback (`onSave_`), there is no `onCancel_` callback to allow callers to handle cancel actions (long-press N when empty) properly. This forces wizard flows (like GPG and TOTP) to rely on generic view stack behavior instead of having explicit cancel handling.

**Location:** `components/cdc_views/include/cdc_views/T9InputView.h`, lines 28-40 (header) and `components/cdc_views/src/T9InputView.cpp`, lines 215-219 (key handling)

## Impact
Without a cancel callback, wizard flows that use T9InputView have limited options for exit handling:

1. **No explicit cancel path**: When a user long-presses 'N' to clear text (line 239-248), there's no way to notify the wizard that cancellation occurred.

2. **Inconsistent UX**: Different modules may implement cancel handling differently, leading to inconsistent user experience.

3. **Trapped wizard states**: As documented in issues #001 (GPG) and #002 (TOTP), wizards built with T9InputView have no clean exit mechanism because the component doesn't expose a cancel callback.

4. **Callback limitation**: The current `onSave_` callback only fires on 'Y' (confirm), not on cancel, so wizards can't distinguish between "user completed" and "user cancelled".

## Evidence
**T9InputView key handling (lines 211-227):**

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

**T9InputView long-press handling (lines 235-253):**

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
    // ... rest of code
}
```

When long-press 'N' clears all text, there's no callback to notify the wizard. The wizard must track state externally or rely on the user to press 'N' again when empty (which returns `REQUEST_POP` but without context).

**Header definition (lines 28-35):**

```cpp
/**
 * Save callback (called when Y is pressed)
 * @param text Final text
 */
using SaveCallback = void(*)(const char* text);

// No CancelCallback defined!
```

## Recommended Fix
Add a cancel callback mechanism to T9InputView:

**Step 1: Add CancelCallback type and member (T9InputView.h):**

```cpp
/**
 * Cancel callback (called when long-press N clears all text)
 */
using CancelCallback = void(*)(void);

void setOnCancel(CancelCallback callback) { onCancel_ = callback; }
```

**Step 2: Update onLongPress to call cancel (T9InputView.cpp):**

```cpp
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
```

**Step 3: Update footer hint to show cancel option:**

```cpp
const char* T9InputView::getFooterHint() const {
    return tr(StringId::HINT_T9_INPUT);  // Update to include "Long-N: Cancel"
}
```

**Step 4: Update GPG and TOTP wizards to use cancel callback:**

```cpp
static void onWizardCancel() {
    // Pop remaining wizard steps
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
    ui::showToast(ui::tr(ui::StringId::CANCELLED), 1000);
}

// In wizardStart():
s_t9Input.setOnCancel(onWizardCancel);
```

## Alternative Approach
If a callback-based approach is too invasive, add a new `InputResult` type for cancel:

```cpp
enum class InputResult {
    IGNORED,
    CONSUMED,
    REQUEST_POP,
    REQUEST_POP_MULTIPLE  // Pop until we reach a specific view
};

// Then in onLongPress:
if (key == 'N' && len_ == 0) {
    return InputResult::REQUEST_POP_MULTIPLE;
}
```

## References
- **Related issues:** #001-gpg-wizard-no-cancel, #002-totp-wizard-no-cancel
- **User Flow Dead Ends:** Input components without cancel mechanism
- **T9InputView API:** `components/cdc_views/include/cdc_views/T9InputView.h`
- **ViewStack API:** `components/cdc_ui/include/cdc_ui/ViewStack.h` - provides `pop()` for navigation
