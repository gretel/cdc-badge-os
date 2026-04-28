---
title: "[MEDIUM] PIN validation errors only shown after submit, no inline feedback during entry"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `PinEntryView.cpp:130-174` and `PinChangeView.cpp:149-225`, PIN validation (minimum length, verification) only occurs when the user presses 'Y' (confirm). There is no inline validation feedback as the user types, and no visual indication of how many digits remain to be entered.

## Impact
- Users may enter a short PIN and only discover the error after pressing confirm
- No visual distinction between required and optional input
- Users cannot see progress toward the minimum length requirement
- Error messages appear in transient toasts (1.5s duration) which may disappear before the user can read them

## Evidence
**File: `components/cdc_views/src/PinEntryView.cpp:130-140`**
```cpp
void PinEntryView::verify() {
    if (length_ < minLength_) {
        if (showMessages_) {
            showMessage(tr(StringId::PIN_TOO_SHORT), MessageIcon::WARNING, 1500);
        }
        return;
    }
    // ...
}
```

**File: `components/cdc_views/src/PinEntryView.cpp:227-260`**
The render function shows filled/empty dots but does not indicate:
- How many digits are required (minLength_)
- Whether the current length meets the minimum
- Real-time validation status

**File: `components/cdc_views/src/PinEntryView.cpp:165-174`**
Error messages use `showMessage()` with 1500ms duration - too brief for users to read and understand.

## Recommended Fix
1. Add visual indicator showing "X/4" or similar progress near the PIN dots
2. Change error message duration from 1500ms to at least 3000ms for readability
3. Consider highlighting the PIN dots in a different color (if display supports it) when minimum length is reached
4. Add a footer hint that updates dynamically (e.g., "4 digits required, 2 entered")

## References
- Nielsen Norman Group: [Inline Validation in Forms](https://www.nngroup.com/articles/inline-validation-in-forms/)
- WCAG 3.3.1: [Error Identification](https://www.w3.org/WAI/WCAG21/Understanding/error-identification.html)
