---
title: "[LOW] Date input view uses hardcoded DD.MM.YYYY order"
severity: LOW
domain: i18n
lens: locale-aware-formatting
labels:
  - "audit:i18n/i18n-formatting"
---

## Summary

The `DateInputView` component uses a fixed day-month-year order with `.` separators. Different locales expect different input orders:
- US: MM/DD/YYYY
- Europe: DD.MM.YYYY
- ISO/Asia: YYYY-MM-DD

**Files and line numbers:**
- `components/cdc_views/src/DateInputView.cpp` - Entire date input implementation
- `components/cdc_os_ui/src/SettingsHandlers.cpp:146-158` - Date confirmation handler

## Impact

Users in different locales may expect a different input order, leading to confusion when entering dates (e.g., entering month as day).

## Evidence

```cpp
// components/cdc_views/src/DateInputView.cpp:35-41
void DateInputView::init(const char* title, uint8_t day, uint8_t month, uint16_t year) {
    title_ = title;
    day_ = (day >= 1 && day <= 31) ? day : 1;
    month_ = (month >= 1 && month <= 12) ? month : 1;
    year_ = year;
    currentField_ = Field::DAY;  // Always starts with DAY
    ...
}
```

The field order is hardcoded as DAY -> MONTH -> YEAR.

## Recommended Fix

1. Add a `DateOrder` enum with values: `DAY_MONTH_YEAR`, `MONTH_DAY_YEAR`, `YEAR_MONTH_DAY`
2. Modify `DateInputView::init()` to accept a date order parameter
3. Use the configured date order from settings when creating the date input view
4. Display hints showing the expected format (e.g., "DD.MM.YYYY" or "MM/DD/YYYY")

## References

- [Unicode LDML Date Formats](https://www.unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table)
