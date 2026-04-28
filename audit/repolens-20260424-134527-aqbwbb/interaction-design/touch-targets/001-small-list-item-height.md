---
title: "[MEDIUM] List item height of 18px provides limited visual feedback for selection"
severity: MEDIUM
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The `ListView` component uses a default item height of **18 pixels** (`DEFAULT_ITEM_HEIGHT = 18` in `components/cdc_views/include/cdc_views/ListView.h:35`). On a display that is only 128 pixels tall, this results in 4 visible items with minimal visual separation between them.

**Evidence:**
- File: `components/cdc_views/include/cdc_views/ListView.h:35`
  ```cpp
  static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 18;
  ```
- File: `components/cdc_views/src/ListView.cpp:21-30`
  ```cpp
  static constexpr int TITLE_Y = 5;
  static constexpr int LIST_START_Y = 30;
  static constexpr int ITEM_PADDING_X = 10;
  static constexpr int SCROLL_INDICATOR_WIDTH = 8;

  /**
   * \brief Visible item count derived from available list area.
   *
   * Available height: 128 - 30 (header) - 16 (footer) = 82px.
   * With `itemHeight_=18`: 82 / 18 = 4 visible rows.
   */
  static constexpr uint8_t VISIBLE_ITEMS = 4;
  ```
- File: `components/cdc_views/src/ListView.cpp:220`
  ```cpp
  gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
  ```
  The selected item highlight is rendered at `itemHeight_ - 2 = 16px` tall.

## Impact
**Visual Clarity:** With only 18px per row and 16px for the selected highlight, users may have difficulty:
1. Quickly identifying which item is currently selected
2. Distinguishing between adjacent items when scrolling
3. Reading text that may be vertically constrained within 18px

**Physical Keypad Context:** Since this device uses a physical 12-button keypad (not touch), the visual feedback on the display is the primary way users understand their current position in a list. Small item heights reduce this clarity.

## Recommended Fix
Increase `DEFAULT_ITEM_HEIGHT` from **18px to 20-22px** to provide better visual separation:

```cpp
// components/cdc_views/include/cdc_views/ListView.h:35
static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 22;  // Increased from 18
```

This would result in:
- 3-4 visible items instead of 4 (depending on exact calculation)
- Larger selected-item highlight area
- Better visual separation between list items
- More comfortable reading area for text

If maintaining 4 visible items is important, consider:
1. Reducing header/footer heights slightly
2. Using a slightly larger item height (20px) and accepting 3-4 items visible

## References
- WCAG 2.5.5 Target Size: Recommends 44x44 CSS pixels for touch targets (adapted for embedded: larger visual targets improve usability)
- Material Design: Recommends 48dp minimum touch target height for lists
- Human Interface Guidelines: Suggests 44pt minimum for iOS list row heights
