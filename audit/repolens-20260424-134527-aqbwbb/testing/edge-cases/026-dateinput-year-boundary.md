---
title: "[MEDIUM] DateInputView year field lacks upper boundary validation on confirm"
severity: MEDIUM
domain: cdc_views/DateInputView
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `DateInputView::onKey` (file: `components/cdc_views/src/DateInputView.cpp:175-195`), when the user presses 'Y' to confirm, the `validateAndClamp()` function is called which clamps year to 2000-2099. However, there is no explicit check to ensure the year is actually valid before confirming.

The `enterDigit` function at lines 98-143 allows entering any 4-digit year without intermediate validation. A user could enter year "9999" and it would only be clamped on confirm, which might be unexpected behavior.

## Impact
- **Data integrity**: Years outside the expected range (2000-2099) might be accepted if user enters them quickly
- **User experience**: No feedback until confirm that the year was out of range
- **Edge case**: Year 0000 could be entered and would be clamped to 2000, but this might not be obvious to users

## Evidence
File: `components/cdc_views/src/DateInputView.cpp`

Lines 98-143 (enterDigit):
```cpp
void DateInputView::enterDigit(char digit) {
    uint8_t d = digit - '0';

    switch (currentField_) {
        case Field::YEAR: {
            if (digitPos_ == 0) {
                year_ = d * 1000;
                digitPos_ = 1;
            } else if (digitPos_ == 1) {
                year_ = (year_ / 1000) * 1000 + d * 100;
                digitPos_ = 2;
            } else if (digitPos_ == 2) {
                year_ = (year_ / 100) * 100 + d * 10;
                digitPos_ = 3;
            } else {
                year_ = (year_ / 10) * 10 + d;
                digitPos_ = 0;  // Wrap around, stay in year
            }
            break;
        }
    }
    dirty_ = true;
}
```

Lines 146-155 (validateAndClamp):
```cpp
bool DateInputView::validateAndClamp() {
    if (day_ < 1) day_ = 1;
    if (day_ > 31) day_ = 31;
    if (month_ < 1) month_ = 1;
    if (month_ > 12) month_ = 12;
    if (year_ < 2000) year_ = 2000;
    if (year_ > 2099) year_ = 2099;
    return true;
}
```

Lines 175-195 (onKey - confirm):
```cpp
case 'Y':  // Confirm
    validateAndClamp();
    LOG_I(TAG, "Date confirmed: %02d.%02d.%04d", day_, month_, year_);
    if (onConfirm_) {
        onConfirm_(day_, month_, year_);
    }
    return InputResult::REQUEST_POP;
```

## Recommended Fix
Add intermediate validation during digit entry to clamp values as they're entered:

```cpp
void DateInputView::enterDigit(char digit) {
    uint8_t d = digit - '0';

    switch (currentField_) {
        case Field::YEAR: {
            if (digitPos_ == 0) {
                year_ = d * 1000;
                digitPos_ = 1;
            } else if (digitPos_ == 1) {
                year_ = (year_ / 1000) * 1000 + d * 100;
                digitPos_ = 2;
            } else if (digitPos_ == 2) {
                year_ = (year_ / 100) * 100 + d * 10;
                digitPos_ = 3;
            } else {
                year_ = (year_ / 10) * 10 + d;
                digitPos_ = 0;  // Wrap around, stay in year
            }
            // Add immediate clamping for year
            if (year_ > 2099) year_ = 2099;
            break;
        }
    }
    dirty_ = true;
}
```

Also consider adding visual feedback when values are clamped.

## References
- CWE-131: Incorrect Calculation of Multi-Byte String Length
- OWASP: Input Validation Cheat Sheet
