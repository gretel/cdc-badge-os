---
title: "[LOW] PIN entry dots at 12px may be too small for clear visual feedback"
severity: LOW
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The PIN entry view uses dots of **12 pixels** diameter (`PIN_DOT_SIZE = 12`) with **16 pixels** spacing (`PIN_DOT_SPACING = 16`) to show entered digits. While space-efficient, these dots may appear small on the 2.9" E-Paper display, especially when viewed from a distance or by users with reduced visual acuity.

**Evidence:**
- File: `components/cdc_os_ui/src/views/PinChangeView.cpp:25-26`
  ```cpp
  static constexpr int PIN_DOT_SIZE = 12;
  static constexpr int PIN_DOT_SPACING = 16;
  ```
- File: `components/cdc_os_ui/src/views/PinChangeView.cpp:350-358`
  ```cpp
  for (int i = 0; i < dotsToShow; i++) {
      int x = startX + i * PIN_DOT_SPACING;
      int y = PIN_Y;

      if (i < length_) {
          gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
      } else {
          gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 2, EPD_BLACK);
      }
  ```

The radius is `PIN_DOT_SIZE / 2 - 1 = 5px` for filled circles and `PIN_DOT_SIZE / 2 - 1 = 5px` (or 6px based on the code) for outlined circles.

## Impact
**Visual Feedback:** Small dots may make it difficult for users to:
1. Quickly see how many digits they've entered
2. Distinguish between filled (entered) and empty (remaining) dots
3. Have a clear sense of progress during PIN entry

**Accessibility:** Users with reduced visual acuity may struggle to see 12px dots clearly, especially on an E-Paper display which has lower contrast than LCD/OLED.

**Consistency:** The PIN dots are smaller than the list item height (18px), creating visual inconsistency in interactive element sizes.

## Recommended Fix
Increase `PIN_DOT_SIZE` from **12px to 16px** and adjust spacing accordingly:

```cpp
// components/cdc_os_ui/src/views/PinChangeView.cpp:25-26
static constexpr int PIN_DOT_SIZE = 16;      // Increased from 12
static constexpr int PIN_DOT_SPACING = 20;   // Increased from 16
```

This would provide:
- Larger, more visible dots for better feedback
- Consistent sizing with other UI elements (closer to 18px list items)
- Better distinction between filled and empty states

If space is constrained, a minimum of **14px** dots with **18px** spacing would still be an improvement.

## References
- WCAG 1.4.11 Non-text Contrast: UI components should have sufficient contrast and size
- Material Design: Form input indicators typically use 16-20dp for visibility
- E-Paper displays benefit from larger, bolder visual elements due to lower contrast ratios
