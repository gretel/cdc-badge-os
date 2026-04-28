---
title: "[LOW] Date input lacks calendar-aware validation (e.g., Feb 30, leap years)"
severity: LOW
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `DateInputView.cpp:145-159`, the date validation only clamps values to broad ranges (day 1-31, month 1-12, year 2000-2099) but doesn't validate calendar-aware constraints like February having 28/29 days, or months with 30 vs 31 days.

## Impact
- Users can enter invalid dates like "31/02/2024" which get silently clamped to "31/02/2024" (still invalid) or "03/02/2024"
- No feedback that the date was adjusted
- Potential data integrity issues if invalid dates are accepted

## Evidence
**File: `components/cdc_views/src/DateInputView.cpp:145-159`**
```cpp
bool DateInputView::validateAndClamp() {
    if (day_ < 1) day_ = 1;
    if (day_ > 31) day_ = 31;  // Doesn't check month-specific max
    if (month_ < 1) month_ = 1;
    if (month_ > 12) month_ = 12;
    if (year_ < 2000) year_ = 2000;
    if (year_ > 2099) year_ = 2099;
    return true;  // Always returns true, even for invalid dates
}
```

**File: `components/cdc_views/src/DateInputView.cpp:98-140`**
The `enterDigit()` function also clamps but doesn't validate month-specific day limits.

## Recommended Fix
1. Add a `isValidDate()` function that checks month-specific day limits and leap years
2. Show a warning or auto-correct invalid dates with a notification
3. Consider preventing the user from entering invalid combinations (e.g., auto-advance to month after entering "29" for February)

## References
- ISO 8601: [Date and Time Format](https://www.iso.org/iso-8601-date-and-time-format.html)
- Nielsen Norman Group: [Date Input Best Practices](https://www.nngroup.com/articles/date-input-forms/)
