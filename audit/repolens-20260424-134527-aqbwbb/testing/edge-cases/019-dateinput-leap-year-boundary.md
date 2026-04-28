---
title: "[MEDIUM] DateInputView accepts invalid February 29th for non-leap years"
severity: MEDIUM
domain: ui/date-validation
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary

The `DateInputView` component (`components/cdc_views/src/DateInputView.cpp`) accepts February 29th as a valid date for any year, including non-leap years. The validation logic at lines 76-88 and 147-156 only performs range checking (day 1-31, month 1-12) without verifying calendar validity.

**Location**: `components/cdc_views/src/DateInputView.cpp:76-88`, `components/cdc_views/src/DateInputView.cpp:147-156`

The `clearField()` function sets day/month/year to 0, and `validateAndClamp()` clamps day to 1-31 and month to 1-2099, but neither function checks if the day is valid for the specific month and year combination.

## Impact

**Data Integrity Risk**: Users can input dates like "29/02/2025" (Feb 29, 2025) which is not a valid calendar date. This can lead to:
- Incorrect date calculations downstream
- Potential issues with TOTP seed expiration dates or other time-sensitive data
- Confusion when the badge displays or processes "invalid" dates

**User Experience**: No feedback is given when an invalid date is entered, making it unclear whether the date was accepted correctly.

## Evidence

**Code at line 76-88** (`clearField()`):
```cpp
void DateInputView::clearField() {
    switch (currentField_) {
        case Field::DAY:
            day_ = 0;
            break;
        case Field::MONTH:
            month_ = 0;
            break;
        case Field::YEAR:
            year_ = 0;
            break;
    }
    digitPos_ = 0;
    dirty_ = true;
}
```

**Code at line 147-156** (`validateAndClamp()`):
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

**Code at line 98-139** (`enterDigit()`):
```cpp
void DateInputView::enterDigit(char digit) {
    // ...
    case Field::DAY: {
        if (digitPos_ == 0) {
            day_ = d * 10;
            digitPos_ = 1;
        } else {
            day_ = (day_ / 10) * 10 + d;
            if (day_ < 1) day_ = 1;
            if (day_ > 31) day_ = 31;  // Only checks 31, not month-specific
            nextField();
        }
        break;
    }
    // ...
}
```

**Test case that currently passes but should fail**:
- Input: Day=29, Month=2, Year=2025 (non-leap year)
- Expected: Rejection or warning
- Actual: Accepted without validation

## Recommended Fix

Add a leap year-aware validation function and integrate it into the confirm flow:

1. **Add leap year helper function**:
```cpp
/**
 * \brief Checks if a year is a leap year.
 * \param year Year to check.
 * \return `true` if leap year.
 */
static bool isLeapYear(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * \brief Returns maximum days in a given month/year.
 * \param month Month (1-12).
 * \param year Year.
 * \return Maximum valid day.
 */
static uint8_t daysInMonth(uint8_t month, uint16_t year) {
    static const uint8_t days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days[month];
}
```

2. **Update `validateAndClamp()` to validate calendar correctness**:
```cpp
bool DateInputView::validateAndClamp() {
    if (day_ < 1) day_ = 1;
    if (month_ < 1) month_ = 1;
    if (year_ < 2000) year_ = 2000;
    if (year_ > 2099) year_ = 2099;
    
    // Clamp month first (needed for day calculation)
    if (month_ > 12) month_ = 12;
    
    // Clamp day to month-specific maximum
    uint8_t maxDay = daysInMonth(month_, year_);
    if (day_ > maxDay) day_ = maxDay;
    
    return true;
}
```

3. **Add validation feedback on confirm** (line 172-178):
```cpp
case 'Y':  // Confirm
    validateAndClamp();
    // Optional: Show warning if date was adjusted
    if (onConfirm_) {
        onConfirm_(day_, month_, year_);
    }
    return InputResult::REQUEST_POP;
```

## References

- [ISO 8601 Date Representation](https://en.wikipedia.org/wiki/ISO_8601)
- [Leap Year Algorithm](https://en.wikipedia.org/wiki/Leap_year#Algorithm)
- C++ Embedded Date/Time Best Practices
