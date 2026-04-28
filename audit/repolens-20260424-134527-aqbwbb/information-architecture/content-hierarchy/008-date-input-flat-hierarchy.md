---
title: "[LOW] DateInputView displays three fields without clear visual hierarchy or grouping"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The `DateInputView` component (`components/cdc_views/src/DateInputView.cpp:1-240`) displays three date fields (day, month, year) in a single flat line (`DD / MM / YYYY`) without:
- Visual grouping or section boundaries between fields
- Clear labels identifying each field (e.g., "Day", "Month", "Year")
- Progressive disclosure or step-by-step input flow indication
- Visual distinction between required fields and their values

The active field is indicated only by a small underline, which may be subtle on a 296x128 display.

## Impact
Users entering dates may:
- Confuse the field order (DD/MM/YY vs MM/DD/YY depending on regional expectations)
- Miss which field is currently active due to subtle underlining
- Have difficulty correcting mistakes when the field boundaries are not visually clear

On a small e-paper display with limited resolution (296x128 pixels), the lack of clear field separation makes the input flow less intuitive.

## Evidence
File: `components/cdc_views/src/DateInputView.cpp:200-230`
```cpp
char dateStr[20];
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);

gfx->setTextSize(2);
// ... center and print date string ...

gfx->fillRect(0, UNDERLINE_Y, width, 4, EPD_WHITE);

// Only a small underline indicates active field
switch (currentField_) {
    case Field::DAY:
        underlineX = startX;
        underlineW = 2 * charWidth;
        break;
    // ...
}
gfx->fillRect(underlineX, UNDERLINE_Y, underlineW, 3, EPD_BLACK);
```

The rendering uses a single text string with underlines for the active field only. No labels or section headers are shown.

## Recommended Fix
1. Add small labels above or below each field (e.g., "DAY", "MON", "YR") to clarify field purpose
2. Increase visual distinction for the active field (e.g., inverse colors, bold text, or thicker underline)
3. Consider adding separators with more visual weight (e.g., vertical bars `|` instead of slashes `/`)

Approximate effort: 45-60 minutes to modify the render function with clearer field indicators.

## References
- DateInputView implementation: `components/cdc_views/src/DateInputView.cpp`
- Similar patterns in `TimeInputView.cpp` (may have same issue)
- Display dimensions: 296x128 pixels (good_display GDEY029T94)
