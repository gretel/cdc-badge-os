---
title: "[MEDIUM] Date and time input views lack clear field-focused indication"
severity: MEDIUM
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `DateInputView.cpp:240-265` and `TimeInputView.cpp:200-220`, the active field is indicated by an underline beneath the current field. However, this underline is subtle and may not be immediately obvious to users, especially on an e-ink display with limited contrast.

## Impact
- Users may be unsure which field they are currently editing
- Field navigation (via keys) may be confusing without clear visual feedback
- On E-Paper display with limited grayscale, the underline contrast may be insufficient

## Evidence
**File: `components/cdc_views/src/DateInputView.cpp:240-265`**
```cpp
gfx->fillRect(0, UNDERLINE_Y, width, 4, EPD_WHITE);  // Clear previous underline

int charWidth = 12;
int underlineX = startX;
int underlineW = 0;

switch (currentField_) {
    case Field::DAY:
        underlineX = startX;
        underlineW = 2 * charWidth;
        break;
    // ...
}

gfx->fillRect(underlineX, UNDERLINE_Y, underlineW, 3, EPD_BLACK);  // Draw underline
```

**File: `components/cdc_views/src/TimeInputView.cpp:200-220`**
Similar implementation with underline only.

## Recommended Fix
1. Add a more prominent indicator (e.g., bold text, inverted background, or blinking cursor) for the active field
2. Consider adding a label below each field (e.g., "DD / MM / YYYY" with the active part highlighted)
3. If space allows, add a brief hint in the footer showing the current field name

## References
- WCAG 1.3.1: [Info and Relationships](https://www.w3.org/WAI/WCAG21/Understanding/info-and-relationships.html)
- Nielsen Norman Group: [Focus Indicators](https://www.nngroup.com/articles/focus-indicators/)
