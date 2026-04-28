---
title: "[MEDIUM] DateInputView and TimeInputView lack arrow-key field navigation"
severity: MEDIUM
domain: interaction-design
lens: keyboard-navigation
labels:
  - keyboard-shortcuts
  - custom-widgets
  - keyboard-accessibility
---

## Summary

The `DateInputView` and `TimeInputView` components require digit-by-digit input with automatic field advancement. However, they lack keyboard shortcuts for field navigation (e.g., arrow keys to move between day/month/year fields). Users must rely on the automatic progression or re-enter data if they make a mistake.

**Evidence locations:**
- `components/cdc_views/src/DateInputView.cpp` - Date input handling (lines 162-195)
- `components/cdc_views/src/TimeInputView.cpp` - Time input handling (lines 125-154)
- `components/cdc_views/src/T9InputView.cpp` - Text input with better navigation (lines 220-250)

For comparison, `T9InputView` has long-press support for clearing all text and force-inserting digits, but `DateInputView` and `TimeInputView` have no equivalent shortcuts.

## Impact

**User Experience Impact:**
- Users cannot easily move between fields without completing the current one
- No way to go back and correct a field without re-entering everything
- Users with motor impairments may find digit-by-digit entry frustrating
- No keyboard efficiency for power users

**Accessibility Impact:**
- Arrow keys are the standard navigation method for form fields
- Screen reader users expect to be able to tab between fields
- No keyboard shortcut for "clear field" or "clear all"

## Evidence

**File: `components/cdc_views/src/DateInputView.cpp` (lines 162-195)**
```cpp
InputResult DateInputView::onKey(char key) {
    // Digit input (all 0-9 keys are digits, auto-advances between fields)
    if (key >= '0' && key <= '9') {
        enterDigit(key);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case 'N':  // Clear or cancel
            if (digitPos_ > 0 ||
                (currentField_ == Field::DAY && day_ > 0) ||
                (currentField_ == Field::MONTH && month_ > 0) ||
                (currentField_ == Field::YEAR && year_ > 0)) {
                clearField();
                return InputResult::CONSUMED;
            }
            return InputResult::REQUEST_POP;

        case 'Y':  // Confirm
            validateAndClamp();
            LOG_I(TAG, "Date confirmed: %02d.%02d.%04d", day_, month_, year_);
            if (onConfirm_) {
                onConfirm_(day_, month_, year_);
            }
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}
```

Only handles digits and N/Y keys. No arrow key navigation.

**File: `components/cdc_views/src/TimeInputView.cpp` (lines 125-154)**
```cpp
InputResult TimeInputView::onKey(char key) {
    // Digit input (all 0-9 keys are digits, auto-advances between fields)
    if (key >= '0' && key <= '9') {
        enterDigit(key);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case 'N':  // Clear or cancel
            if (digitPos_ > 0 ||
                (currentField_ == Field::HOUR && hour_ > 0) ||
                (currentField_ == Field::MINUTE && minute_ > 0)) {
                clearField();
                return InputResult::CONSUMED;
            }
            return InputResult::REQUEST_POP;

        case 'Y':  // Confirm
            if (hour_ > 23) hour_ = 23;
            if (minute_ > 59) minute_ = 59;
            LOG_I(TAG, "Time confirmed: %02d:%02d", hour_, minute_);
            if (onConfirm_) {
                onConfirm_(hour_, minute_);
            }
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}
```

Same limitation - no arrow key navigation.

**Comparison: T9InputView with better shortcuts (lines 238-258)**
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

    if (key >= '0' && key <= '9') {
        // Force insert digit
        forceDigit(key);
        return InputResult::CONSUMED;
    }

    return InputResult::IGNORED;
}
```

T9InputView has long-press shortcuts for clearing and force-insertion.

## Recommended Fix

### 1. Add Arrow Key Navigation

For `DateInputView`:
```cpp
InputResult DateInputView::onKey(char key) {
    // Digit input
    if (key >= '0' && key <= '9') {
        enterDigit(key);
        return InputResult::CONSUMED;
    }

    // Arrow key navigation
    switch (key) {
        case '4':  // Left = previous field
            prevField();
            return InputResult::CONSUMED;

        case '6':  // Right = next field
            nextField();
            return InputResult::CONSUMED;

        case 'N':  // Clear or cancel
            // ... existing code ...

        case 'Y':  // Confirm
            // ... existing code ...

        default:
            return InputResult::IGNORED;
    }
}
```

For `TimeInputView`:
```cpp
InputResult TimeInputView::onKey(char key) {
    // Digit input
    if (key >= '0' && key <= '9') {
        enterDigit(key);
        return InputResult::CONSUMED;
    }

    // Arrow key navigation
    switch (key) {
        case '4':  // Left = previous field
            prevField();
            return InputResult::CONSUMED;

        case '6':  // Right = next field
            nextField();
            return InputResult::CONSUMED;

        case 'N':  // Clear or cancel
            // ... existing code ...

        case 'Y':  // Confirm
            // ... existing code ...

        default:
            return InputResult::IGNORED;
    }
}
```

### 2. Add Long-Press Shortcuts

```cpp
InputResult DateInputView::onLongPress(char key) {
    if (key == 'N') {
        // Clear all fields
        day_ = 1;
        month_ = 1;
        year_ = 2024;
        currentField_ = Field::DAY;
        digitPos_ = 0;
        dirty_ = true;
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}
```

### 3. Update Documentation

```cpp
/**
 * DateInputView - Date entry widget
 *
 * Keys:
 *   0-9 = Enter digits (auto-advances)
 *   4 = Previous field (Left)
 *   6 = Next field (Right)
 *   N = Clear current field / Cancel
 *   Y = Confirm
 *
 * Long-press:
 *   N = Clear all fields
 */
```

## References

- WAI-ARIA Authoring Practices for Date Picker: https://www.w3.org/WAI/ARIA/apg/patterns/dialog-modal/examples/date-picker-dialog/
- ESP32-S3 CDC Badge 12-button keypad layout
- Legacy implementation reference: `~/GIT/cdc-badge-os-legacy/main/app_input.cpp`
