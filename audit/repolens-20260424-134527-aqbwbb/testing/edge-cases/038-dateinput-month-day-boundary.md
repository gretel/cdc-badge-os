---
title: "[MEDIUM] DateInputView validates day/month independently but doesn't check calendar validity"
severity: MEDIUM
domain: cdc_views/DateInputView
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `DateInputView.cpp` (file: `components/cdc_views/src/DateInputView.cpp:35-159`), the `init` and `validateAndClamp` functions validate day and month independently, but don't check if the day is valid for the given month (e.g., February 30, April 31).

Lines 35-39:
```cpp
void DateInputView::init(const char* title, uint8_t day, uint8_t month, uint16_t year) {
    title_ = title;
    day_ = (day >= 1 && day <= 31) ? day : 1;
    month_ = (month >= 1 && month <= 12) ? month : 1;
    year_ = year;
    // ...
}
```

Lines 145-159:
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

The validation at lines 36-37 and 146-149 only checks that day is 1-31 and month is 1-12, but doesn't verify that the day is valid for the specific month. For example:
- February (month 2) can have at most 29 days
- April, June, September, November have at most 30 days
- January, March, May, July, August, October, December have 31 days

## Impact
- **Invalid dates accepted**: Users can enter dates like "February 30" or "April 31"
- **mktime() behavior undefined**: When `validateAndClamp()` passes an invalid date to `mktime()` in `SettingsHandlers.cpp`, the behavior may be platform-dependent
- **Data integrity**: Invalid dates may be persisted to the RTC or used in calculations

## Evidence
File: `components/cdc_views/src/DateInputView.cpp`, lines 35-39 and 145-159

```cpp
void DateInputView::init(const char* title, uint8_t day, uint8_t month, uint16_t year) {
    title_ = title;
    day_ = (day >= 1 && day <= 31) ? day : 1;  // Line 36: No month-aware check
    month_ = (month >= 1 && month <= 12) ? month : 1;
    year_ = year;
    currentField_ = Field::DAY;
    digitPos_ = 0;
    dirty_ = true;
}

bool DateInputView::validateAndClamp() {
    if (day_ < 1) day_ = 1;
    if (day_ > 31) day_ = 31;  // Line 147: No month-aware check
    if (month_ < 1) month_ = 1;
    if (month_ > 12) month_ = 12;
    if (year_ < 2000) year_ = 2000;
    if (year_ > 2099) year_ = 2099;
    return true;
}
```

The edge cases not handled:
- February 29 on non-leap years
- February 30 (always invalid)
- April 31, June 31, September 31, November 31 (months with 30 days)

## Recommended Fix
Add month-aware day validation:

```cpp
/**
 * \brief Returns maximum days for a given month and year.
 * \param month Month (1-12).
 * \param year Year (for leap year calculation).
 * \return Maximum days in the month.
 */
static uint8_t daysInMonth(uint8_t month, uint16_t year) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return 31;
    
    uint8_t maxDays = days[month - 1];
    
    // Check for leap year (February only)
    if (month == 2) {
        bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (isLeap) maxDays = 29;
    }
    
    return maxDays;
}

void DateInputView::init(const char* title, uint8_t day, uint8_t month, uint16_t year) {
    title_ = title;
    uint8_t maxDay = daysInMonth(month, year);
    day_ = (day >= 1 && day <= maxDay) ? day : 1;
    month_ = (month >= 1 && month <= 12) ? month : 1;
    year_ = year;
    currentField_ = Field::DAY;
    digitPos_ = 0;
    dirty_ = true;
}

bool DateInputView::validateAndClamp() {
    if (day_ < 1) day_ = 1;
    if (month_ < 1) month_ = 1;
    if (month_ > 12) month_ = 12;
    if (year_ < 2000) year_ = 2000;
    if (year_ > 2099) year_ = 2099;
    
    // Month-aware day validation
    uint8_t maxDay = daysInMonth(month_, year_);
    if (day_ > maxDay) day_ = maxDay;
    
    return true;
}
```

Add unit tests for:
- February 29 on leap year (2024, 2000)
- February 29 on non-leap year (2023, 2100)
- April 30 vs April 31
- All month boundaries

## References
- POSIX mktime() specification: https://pubs.opengroup.org/onlinepubs/9699919799/functions/mktime.html
- ISO 8601 date format
- Leap year rules: https://en.wikipedia.org/wiki/Leap_year#Gregorian_calendar
- CWE-131: Incorrect Calculation of Multi-Byte String Length
