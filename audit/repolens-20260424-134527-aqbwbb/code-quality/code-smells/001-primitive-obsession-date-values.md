---
title: "[MEDIUM] Primitive Obsession: Date values passed as separate primitives instead of Date struct"
severity: MEDIUM
domain: ui
lens: code-smells
labels:
  - "primitive-obsession"
---

## Summary
In `components/cdc_views/`, the `DateInputView` class uses separate primitive parameters (`uint8_t day`, `uint8_t month`, `uint16_t year`) for date values instead of a dedicated `Date` value object. This pattern appears in:
- `DateInputView::init()` at line 35
- `DateInputView::onConfirm_` callback at line 26
- Multiple calls throughout `AppUi.cpp` and `SettingsHandlers.cpp`

## Impact
**Maintenance burden**: Every function dealing with dates must:
- Accept 4+ parameters (title + 3 date components)
- Maintain correct parameter order across call sites
- Validate date components independently
- Risk mixing up month/day or passing invalid combinations

**Validation scattered**: Date validation logic (e.g., day 1-31, month 1-12) is duplicated in multiple places instead of being encapsulated.

**Type safety**: No compile-time guarantee that 3 primitives represent a coherent date value.

## Evidence
`components/cdc_views/include/cdc_views/DateInputView.h:26`:
```cpp
using ConfirmCallback = void(*)(uint8_t day, uint8_t month, uint16_t year);
```

`components/cdc_views/include/cdc_views/DateInputView.h:35`:
```cpp
void init(const char* title, uint8_t day, uint8_t month, uint16_t year);
```

Similar pattern in `TimeInputView.h:25-33` with hour/minute:
```cpp
using ConfirmCallback = void(*)(uint8_t hour, uint8_t minute);
void init(const char* title, uint8_t hour, uint8_t minute);
```

## Recommended Fix
1. Create a `Date` value object in `components/cdc_views/include/cdc_views/`:
```cpp
struct Date {
    uint8_t day;
    uint8_t month;
    uint16_t year;
    
    /** \brief Validates date is calendar-valid. */
    bool isValid() const;
    /** \brief Returns days in current month. */
    uint8_t daysInMonth() const;
};
```

2. Update `DateInputView` to use `Date`:
```cpp
using ConfirmCallback = void(*)(Date date);
void init(const char* title, Date date);
```

3. Add helper functions for common date operations (leap year, day-of-year, etc.)

**Estimated effort**: ~1 hour to create the struct, update signatures, and fix call sites.

## References
- Refactoring.com: "Primitive Obsession" - https://refactoring.com/catalog/replacePrimitiveWithObject
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 6
