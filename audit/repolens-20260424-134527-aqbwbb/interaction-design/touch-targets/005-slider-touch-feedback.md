---
title: "[LOW] Slider bar lacks clear visual feedback for current position"
severity: LOW
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The `SliderView` component displays a progress bar with a filled portion showing the current value, but the **slider thumb/indicator** is not explicitly drawn. The filled rectangle ends at the current position, but there's no distinct visual marker showing exactly where the current value is.

**Evidence:**
- File: `components/cdc_views/src/SliderView.cpp:22-26`
  ```cpp
  static constexpr int TITLE_Y = 20;
  static constexpr int VALUE_Y = 55;
  static constexpr int BAR_Y = 85;
  static constexpr int BAR_HEIGHT = 20;
  static constexpr int BAR_MARGIN = 20;
  ```
- File: `components/cdc_views/src/SliderView.cpp:181-189`
  ```cpp
  int barWidth = width - 2 * BAR_MARGIN;
  gfx->drawRect(BAR_MARGIN, BAR_Y, barWidth, BAR_HEIGHT, EPD_BLACK);

  int fillWidth = 0;
  if (maxValue_ > minValue_) {
      fillWidth = (value_ - minValue_) * (barWidth - 4) / (maxValue_ - minValue_);
  }
  gfx->fillRect(BAR_MARGIN + 2, BAR_Y + 2, fillWidth, BAR_HEIGHT - 4, EPD_BLACK);
  ```

The slider draws:
1. A border rectangle around the bar
2. A filled rectangle from the left to the current value

But there's **no vertical line, dot, or thumb** at the current value position to clearly mark it.

## Impact
**Visual Clarity:** Without a distinct marker at the current value:
1. Users may have difficulty seeing the exact current position
2. Small values (near 0) may appear as "empty" with no clear marker
3. The transition between filled and empty is a simple edge, not a distinct marker

**Precision:** For users adjusting values, a clear thumb/marker helps understand:
- Where the current value sits
- How close they are to min/max
- The exact position within the bar

## Recommended Fix
Add a vertical line or small thumb marker at the current value position:

```cpp
// components/cdc_views/src/SliderView.cpp:189 (after drawing the filled rect)

// Draw vertical marker at current value position
int markerX = BAR_MARGIN + 2 + fillWidth;
gfx->drawFastVLine(markerX, BAR_Y + 2, BAR_HEIGHT - 4, EPD_BLACK);
// Or draw a small diamond/circle for more prominence
gfx->fillCircle(markerX, BAR_Y + BAR_HEIGHT / 2, 3, EPD_BLACK);
```

This would provide:
- Clear visual marker at the current value
- Better precision for understanding the slider position
- Improved visual feedback for value adjustments

## References
- Material Design: Sliders should have a clear thumb indicating current value
- WAI-ARIA Authoring Practices: Range sliders need clear visual indicators
- Visual feedback: Interactive elements benefit from distinct markers showing current state
