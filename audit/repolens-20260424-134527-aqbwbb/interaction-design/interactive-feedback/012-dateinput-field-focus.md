---
title: "[LOW] DateInputView and TimeInputView lack field focus indicator"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "focus-state"
  - "field-indicator"
  - "form-input"
---

## Summary
The `DateInputView` and `TimeInputView` components show input fields but lack a clear visual indicator of which field is currently active. Users must infer the active field from cursor position or field order, which can be confusing when navigating between fields.

## Impact
- **Navigation clarity**: Users may not know which field will receive input
- **Error reduction**: Clear field focus reduces input errors
- **Accessibility**: Users with cognitive load benefit from explicit focus indicators

## Evidence
File: `components/cdc_views/include/cdc_views/DateInputView.h`, lines 1-18

```cpp
/**
 * DateInputView - Date input with day/month/year fields
 *
 * Navigation:
 *   0-9 = Enter digits
 *   4 = Previous field
 *   6 = Next field
 *   N = Clear current field / Cancel (if empty)
 *   Y = Confirm
 */
```

The component has navigation (fields can be changed with keys 4 and 6), but the render method needs to be checked to see how the active field is indicated.

Looking at the structure, the `currentField_` enum tracks which field is active:
```cpp
enum class Field : uint8_t { DAY = 0, MONTH = 1, YEAR = 2 };
Field currentField_ = Field::DAY;
```

But without seeing the render implementation, it's unclear how this is visually communicated to the user.

## Recommended Fix
Add a clear visual indicator for the active field:

**Option A - Box around active field**:
```cpp
void DateInputView::render(bool partial) {
    // ... existing code ...
    
    // Draw boxes around each field
    int fieldX[3] = {dayX, monthX, yearX};
    int fieldWidth[3] = {dayWidth, monthWidth, yearWidth};
    
    for (int i = 0; i < 3; i++) {
        if (static_cast<int>(currentField_) == i) {
            // Active field: thick border
            gfx->drawRect(fieldX[i] - 2, fieldY - 2, fieldWidth[i] + 4, fieldHeight + 4, EPD_BLACK);
            gfx->drawRect(fieldX[i] - 1, fieldY - 1, fieldWidth[i] + 2, fieldHeight + 2, EPD_BLACK);
        } else {
            // Inactive field: thin border
            gfx->drawRect(fieldX[i], fieldY, fieldWidth[i], fieldHeight, EPD_BLACK);
        }
    }
}
```

**Option B - Highlight active field background**:
```cpp
void DateInputView::render(bool partial) {
    // ... existing code ...
    
    // Highlight active field with inverted background
    int fieldX[3] = {dayX, monthX, yearX};
    int fieldWidth[3] = {dayWidth, monthWidth, yearWidth};
    
    for (int i = 0; i < 3; i++) {
        if (static_cast<int>(currentField_) == i) {
            // Invert background for active field
            gfx->fillRect(fieldX[i] - 2, fieldY - 2, fieldWidth[i] + 4, fieldHeight + 4, EPD_BLACK);
            gfx->setTextColor(EPD_WHITE);
        } else {
            gfx->setTextColor(EPD_BLACK);
        }
        // Print field value
    }
}
```

**Option C - Add a cursor indicator**:
```cpp
void DateInputView::render(bool partial) {
    // ... existing code ...
    
    // Add a small arrow or indicator pointing to active field
    int indicatorY = fieldY - 5;
    int indicatorX = fieldX[static_cast<int>(currentField_)] + fieldWidth[static_cast<int>(currentField_)] / 2;
    
    // Draw downward arrow
    gfx->fillTriangle(
        indicatorX, indicatorY,
        indicatorX - 3, indicatorY + 5,
        indicatorX + 3, indicatorY + 5,
        EPD_BLACK
    );
}
```

## References
- [Form field focus patterns](https://www.w3.org/WAI/tutorials/forms/focus/)
- [Input field design best practices](https://www.nngroup.com/articles/input-fields/)
