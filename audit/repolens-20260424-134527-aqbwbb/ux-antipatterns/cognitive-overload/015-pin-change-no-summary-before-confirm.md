---
title: "[LOW] PIN change wizard doesn't show confirmation details"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - "form-fatigue"
  - "wizard-flow"
---

## Summary
The `PinChangeView` in `components/cdc_os_ui/src/views/PinChangeView.cpp` is a 3-step wizard (Current PIN → New PIN → Confirm PIN) but doesn't show a summary or confirmation dialog before actually changing the PIN. Users might accidentally trigger a PIN change without explicit confirmation.

**File:** `components/cdc_os_ui/src/views/PinChangeView.cpp`
**Lines:** 145-225 (confirmStep function)

## Impact
- **Accidental Changes:** After entering all 3 steps, the PIN changes immediately without a "Are you sure?" confirmation
- **No Review:** Users cannot see what they're about to change (though only the PIN itself is changing)
- **Recovery Cost:** If a user forgets the new PIN, they need to know the old one and go through the process again

## Evidence
The wizard flow in `confirmStep()` transitions through steps and changes PIN directly:

```cpp
// Line 145-225: No final confirmation before changing
void PinChangeView::confirmStep() {
    switch (step_) {
        case Step::CURRENT_PIN: {
            // Verify current PIN
            bool ok = onVerify_ ? onVerify_(currentPin_) : core::PinManager::instance().verifyBadgePin(currentPin_);
            // ...
            step_ = Step::NEW_PIN;  // Move to next step
            break;
        }

        case Step::NEW_PIN: {
            // ...
            step_ = Step::CONFIRM_PIN;  // Move to next step
            break;
        }

        case Step::CONFIRM_PIN: {
            // Check if PINs match
            if (strcmp(newPin_, confirmPin_) != 0) {
                // ...
                step_ = Step::NEW_PIN;  // Go back
                return;
            }

            // Change the PIN immediately!
            bool changed = onChange_
                ? onChange_(currentPin_, newPin_)
                : core::PinManager::instance().setBadgePin(newPin_);
            if (changed) {
                pinChanged_ = true;
                showMessage(tr(StringId::PIN_CHANGED));
            }
            break;
        }
    }
}
```

After step 3 (Confirm PIN), the PIN is changed immediately with no "Y=Confirm, N=Cancel" dialog.

## Recommended Fix
Add a confirmation step before changing the PIN:

```cpp
case Step::CONFIRM_PIN: {
    // Check if PINs match
    if (strcmp(newPin_, confirmPin_) != 0) {
        showMessage(tr(StringId::PIN_MISMATCH));
        step_ = Step::NEW_PIN;
        memset(newPin_, 0, sizeof(newPin_));
        clearBuffer();
        return;
    }

    // Show confirmation dialog
    step_ = Step::CONFIRM_CHANGE;
    showConfirm("Change PIN to " XXXX "?",  // Masked new PIN
                [](void* userData) {
                    // Apply the change
                    auto* view = static_cast<PinChangeView*>(userData);
                    view->applyPinChange();
                },
                [](void* userData) {
                    // Cancel, go back to confirm step
                    auto* view = static_cast<PinChangeView*>(userData);
                    view->step_ = PinChangeView::Step::CONFIRM_PIN;
                },
                ConfirmView::Icon::QUESTION);
    break;
}
```

Or simpler: Show the masked PIN in a message box before confirming:
```cpp
case Step::CONFIRM_PIN: {
    // Show summary
    char summary[48];
    snprintf(summary, sizeof(summary), "New PIN: %s\n\nChange?",
             maskPin(newPin_));  // Show as "****"
    showConfirm(summary, applyPinChange, nullptr, ConfirmView::Icon::QUESTION);
    break;
}
```

## References
- Error Prevention: Confirm before important changes
- Visibility of System Status: Show what will happen before doing it
- Form Design: Summary step for multi-step forms with irreversible actions
