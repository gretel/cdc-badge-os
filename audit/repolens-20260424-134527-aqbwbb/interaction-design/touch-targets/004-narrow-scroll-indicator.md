---
title: "[LOW] Scroll indicator width of 8px is narrow for visual clarity"
severity: LOW
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The scroll indicator on the right side of lists has a width of only **8 pixels** (`kScrollIndicatorWidth = 8`). This narrow width accommodates the scroll arrows and scrollbar thumb but may appear visually thin and less prominent.

**Evidence:**
- File: `components/cdc_views/include/cdc_views/RenderHelpers.h:10`
  ```cpp
  constexpr int kScrollIndicatorWidth = 8;
  ```
- File: `components/cdc_views/src/RenderHelpers.cpp:90-130`
  ```cpp
  void drawScrollIndicator(Gdey029T94* gfx, int x, int y, int listHeight,
                           uint16_t totalItems, uint16_t visibleItems,
                           uint16_t scrollPos) {
      if (!gfx) return;
      if (totalItems <= visibleItems) return;

      gfx->fillRect(x, y, kScrollIndicatorWidth, listHeight, EPD_WHITE);

      const int midX = x + (kScrollIndicatorWidth / 2);  // midX = x + 4
      const int leftX = x + 1;
      const int rightX = x + kScrollIndicatorWidth - 1;  // rightX = x + 7

      // Scroll arrows (triangles)
      if (scrollPos > 0) {
          const int topY = y + 4;
          gfx->fillTriangle(
              midX, topY,
              leftX, topY + 6,
              rightX, topY + 6,
              EPD_BLACK
          );
      }

      // Scrollbar thumb
      const int barX = x + (kScrollIndicatorWidth / 2) - 2;  // barX = x + 2
      gfx->drawRect(barX, barY, 4, barHeight, EPD_BLACK);    // 4px wide thumb
      gfx->fillRect(barX, barY + thumbPos, 4, thumbHeight, EPD_BLACK);
  ```

The scrollbar thumb is only **4 pixels wide** (calculated as `kScrollIndicatorWidth / 2 - 2 = 2`, then drawn with width 4).

## Impact
**Visual Prominence:** The narrow scroll indicator may be:
1. Less noticeable to users, especially at a glance
2. Harder to see when the list has many items (small thumb)
3. Visually thin compared to other UI elements

**Usability:** While the scroll indicator is primarily visual (not touchable since this is a physical keypad device), a more prominent indicator provides better feedback about:
- Whether scrolling is available
- Current position in the list
- How much content remains

## Recommended Fix
Increase `kScrollIndicatorWidth` from **8px to 10-12px** for better visibility:

```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h:10
constexpr int kScrollIndicatorWidth = 10;  // Increased from 8
```

This would provide:
- More prominent scroll arrows
- Wider scrollbar thumb (now 5-6px instead of 4px)
- Better visual balance with the rest of the UI

If the 8px width was chosen to maximize list space, consider that users benefit more from clear scroll feedback than from an extra 2px of list width.

## References
- Nielsen Norman Group: Scroll indicators should be clearly visible to communicate scrollable content
- Material Design: Scrollbar tracks are typically 4-8dp with thumbs at 4dp minimum
- Visual hierarchy: Interactive feedback elements should be prominent enough to notice at a glance
