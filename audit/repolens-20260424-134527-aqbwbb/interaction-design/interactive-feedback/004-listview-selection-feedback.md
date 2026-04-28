---
title: "[MEDIUM] ListView lacks visual transition feedback during navigation"
severity: MEDIUM
domain: cdc_views
lens: interactive-feedback
labels:
  - "selection-state"
  - "visual-feedback"
  - "epd-optimization"
---

## Summary
The `ListView` component (`components/cdc_views/src/ListView.cpp:215-225`) provides selection feedback by inverting colors for the selected item, but there is no visual transition effect when the selection changes. On E-Paper displays with partial updates, the change can be subtle and may be missed by users.

## Impact
- **User confidence**: Users may not immediately perceive that their navigation key press was registered
- **E-Paper optimization**: Partial updates are used for efficiency, but without transition feedback, users might think the input was missed
- **Accessibility**: Users with visual impairments may have difficulty tracking the selection as it moves

## Evidence
File: `components/cdc_views/src/ListView.cpp`, lines 215-225

```cpp
if (isSelected) {
    gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
} else {
    gfx->setTextColor(EPD_BLACK);
}
```

The selection is rendered with inverted colors (black background, white text), but:
- No animation or transition effect between states
- No border or highlight around the selected item
- Previous selection area is cleared without any transitional visual cue

## Recommended Fix
Add a temporary visual indicator for the selection transition:

1. **Option A (Simple)**: Add a border around the selected item
```cpp
if (isSelected) {
    gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
    gfx->drawRect(1, y, rowWidth, itemHeight_, EPD_WHITE); // White border on black background
}
```

2. **Option B (Enhanced)**: Add a small icon or arrow next to the selected item
```cpp
if (isSelected) {
    gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
    // Add a small triangle indicator on the left
    gfx->fillTriangle(4, y + 4, 4, y + 14, 8, y + 9, EPD_WHITE);
}
```

3. **Option C (Transition flash)**: Briefly flash the newly selected item
```cpp
// In navigate() function, after updating selection_
dirty_ = true;
// Add a flag to indicate a flash should occur on next render
flashSelection_ = true;
```

## References
- E-Paper display best practices for partial updates
- [Adafruit GFX library - drawRect](https://learn.adafruit.com/adafruit-gfx-graphics-library/graphics-primitives)
