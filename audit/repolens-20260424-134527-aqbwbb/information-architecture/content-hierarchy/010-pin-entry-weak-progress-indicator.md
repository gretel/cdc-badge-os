---
title: "[LOW] PinEntryView uses uniform dots without clear progress indication for entered digits"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The `PinEntryView` component (`components/cdc_views/src/PinEntryView.cpp:160-240`) displays PIN entry using a series of uniform circles where entered digits are filled and empty positions are outlined. However, this design lacks:
- Clear visual progress indication (no numbering or counter)
- Distinction between "active" position (where next digit will go) vs. already-entered digits
- Section grouping for the PIN entry area
- Clear feedback for the most recently entered digit

All dots are visually identical except for filled vs. empty, making it harder for users to quickly assess how many digits they've entered vs. how many remain.

## Impact
Users entering a PIN may:
- Lose track of how many digits they've entered, especially if distracted
- Not know which position is "active" for the next digit
- Have difficulty verifying their entry on a small display (296x128 pixels)
- Make more errors due to weak visual hierarchy in the progress indicator

The current design assumes users remember their place, which increases cognitive load during PIN entry.

## Evidence
File: `components/cdc_views/src/PinEntryView.cpp:185-205`
```cpp
// PIN dots (centered)
int totalWidth = maxLength_ * PIN_DOT_SIZE + (maxLength_ - 1) * (PIN_DOT_SPACING - PIN_DOT_SIZE);
int startX = (width - totalWidth) / 2;

for (uint8_t i = 0; i < maxLength_; i++) {
    int x = startX + i * PIN_DOT_SIZE;
    int y = PIN_Y;

    if (i < length_) {
        // Filled dot for entered digits
        gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
    } else {
        // Empty dot for remaining positions
        gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
    }
}
```

The rendering uses identical circles with only filled vs. empty distinction. No numbering, highlighting, or progress indicator is shown.

## Recommended Fix
1. Add a small progress indicator below the dots (e.g., "3/6" showing entered/total)
2. Highlight the next empty dot with a different style (e.g., dashed outline, brighter border)
3. Consider adding a subtle animation or flash effect when a digit is entered (if display supports it)

Approximate effort: 45-60 minutes to add a progress counter and enhance the active position indicator.

## References
- PinEntryView implementation: `components/cdc_views/src/PinEntryView.cpp`
- Similar patterns: T9InputView, DateInputView, TimeInputView (all use implicit position tracking)
- Display dimensions: 296x128 pixels (good_display GDEY029T94)
