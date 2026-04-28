---
title: "[LOW] TimeInputView displays two fields without clear visual hierarchy or labels"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The `TimeInputView` component (`components/cdc_views/src/TimeInputView.cpp:1-210`) displays two time fields (hour, minute) in a single flat line (`HH : MM`) without:
- Visual labels identifying each field (e.g., "Hour", "Minute" or "H", "M")
- Clear section boundaries between hour and minute
- Progressive disclosure indication for multi-step input
- Visual distinction between the active field beyond a subtle underline

The active field is indicated only by a small underline beneath the time display, which may be difficult to notice on a 296x128 display.

## Impact
Users entering time may:
- Have difficulty identifying which field is currently active
- Confuse the input flow (especially if coming from DateInputView which uses a similar pattern)
- Make errors when correcting values due to unclear field boundaries

On a small e-paper display with limited resolution (296x128 pixels), the lack of clear field indicators makes the input flow less intuitive, especially for first-time users.

## Evidence
File: `components/cdc_views/src/TimeInputView.cpp:170-205`
```cpp
char timeStr[12];
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);

gfx->setTextSize(3);  // Large text
gfx->getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
int startX = (width - w) / 2;
gfx->setCursor(startX, TIME_Y);
gfx->print(timeStr);

// Clear background for underline area
gfx->fillRect(0, UNDERLINE_Y, width, 4, EPD_WHITE);

// Only a small underline indicates active field
int charWidth = 18;
int underlineX = startX;
int underlineW = 2 * charWidth;

switch (currentField_) {
    case Field::HOUR:
        underlineX = startX;
        break;
    case Field::MINUTE:
        underlineX = startX + 5 * charWidth;
        break;
}
gfx->fillRect(underlineX, UNDERLINE_Y, underlineW, 3, EPD_BLACK);
```

The rendering uses a single large text string with only a subtle underline for the active field. No labels or field names are shown.

## Recommended Fix
1. Add small field labels below each value (e.g., "HR" and "MN" or "H" and "M")
2. Increase visual distinction for the active field (e.g., inverse colors, bold text, or a box around the active field)
3. Consider using a colon separator with more visual weight or different styling

Approximate effort: 45-60 minutes to modify the render function with clearer field indicators. This can be done in parallel with the DateInputView fix (finding #8).

## References
- TimeInputView implementation: `components/cdc_views/src/TimeInputView.cpp`
- Related issue: DateInputView has similar pattern (see finding #8)
- Display dimensions: 296x128 pixels (good_display GDEY029T94)
