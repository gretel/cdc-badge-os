---
title: "[LOW] List items have minimal vertical spacing (no explicit gaps)"
severity: LOW
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The `ListView` component renders list items with **no explicit vertical spacing** between them. Items are rendered back-to-back with only the 18px height defining the boundary. This can make adjacent items appear visually crowded.

**Evidence:**
- File: `components/cdc_views/src/ListView.cpp:205-220`
  ```cpp
  for (uint8_t i = 0; i < visibleItems_; i++) {
      uint16_t itemIndex = scrollPos_ + i;
      int y = LIST_START_Y + i * itemHeight_;  // No spacing calculation

      // Clear item area
      gfx->fillRect(0, y, rowWidth, itemHeight_, EPD_WHITE);

      if (itemIndex >= itemCount_) continue;

      const ListItem& item = items_[itemIndex];
      bool isSelected = (itemIndex == selection_);

      if (isSelected) {
          gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
          gfx->setTextColor(EPD_WHITE);
      } else {
          gfx->setTextColor(EPD_BLACK);
      }
  ```

The items are drawn at `y = LIST_START_Y + i * itemHeight_` with no additional spacing. The selected item has a 1px border inside (`y + 1` to `y + itemHeight_ - 2`), but unselected items have no visual separation.

## Impact
**Visual Grouping:** Without spacing between items:
1. Adjacent items may appear to blend together
2. The list may look dense and harder to scan
3. Visual hierarchy is reduced

**Selection Clarity:** When an item is selected, the highlight fills almost the entire item area (16px of 18px). When the selection moves, the previous item's highlight disappears completely, which can feel abrupt.

## Recommended Fix
Add a 1-2px vertical gap between list items by adjusting the item height calculation:

```cpp
// components/cdc_views/include/cdc_views/ListView.h:35
static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 20;  // Increased from 18
static constexpr uint8_t ITEM_SPACING = 2;          // New constant
```

Then in the render function:
```cpp
// components/cdc_views/src/ListView.cpp:207-209
for (uint8_t i = 0; i < visibleItems_; i++) {
    uint16_t itemIndex = scrollPos_ + i;
    int y = LIST_START_Y + i * (itemHeight_ + ITEM_SPACING);  // Add spacing
```

Or alternatively, draw a 1px separator line between items:
```cpp
// After drawing each item (except the last visible one)
if (i < visibleItems_ - 1 && scrollPos_ + i + 1 < itemCount_) {
    gfx->drawFastHLine(0, LIST_START_Y + (i + 1) * itemHeight_ - 1, rowWidth, EPD_DARKGREY);
}
```

This would provide:
- Better visual separation between list items
- Easier scanning of list contents
- Improved visual hierarchy

## References
- Material Design: Lists typically have 8dp spacing between items or use dividers
- Nielsen Norman Group: White space improves readability and visual grouping
- Visual hierarchy: Spacing helps users distinguish between separate items
