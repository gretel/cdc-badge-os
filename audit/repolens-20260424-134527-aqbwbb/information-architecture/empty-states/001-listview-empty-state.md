---
title: "[MEDIUM] ListView renders completely blank when empty without guidance"
severity: MEDIUM
domain: ui-framework
lens: empty-states
labels:
  - "empty-state"
  - "listview"
  - "user-experience"
---

## Summary
The `ListView` component in `components/cdc_views/src/ListView.cpp` renders a completely blank white area when `itemCount_` is 0, with only the title bar and footer visible. No empty state message, icon, or call-to-action is provided to guide the user.

**Location:** `components/cdc_views/src/ListView.cpp:199-255` (render function)

## Impact
Users staring at a blank list view may:
- Think the application is buggy or frozen
- Be confused about whether data should be there
- Not know what action to take to populate the list
- Exit the view thinking nothing is available

This is particularly relevant for modules like TOTP, Password, and FIDO2 where users expect to see their stored items.

## Evidence
In `ListView::render()` (lines 199-255):
```cpp
// Items
const int rowWidth = width - SCROLL_INDICATOR_WIDTH;
for (uint8_t i = 0; i < visibleItems_; i++) {
    uint16_t itemIndex = scrollPos_ + i;
    int y = LIST_START_Y + i * itemHeight_;

    // Clear item area
    gfx->fillRect(0, y, rowWidth, itemHeight_, EPD_WHITE);

    if (itemIndex >= itemCount_) continue;  // <-- Just skips, no empty state

    // ... renders item ...
}
```

When `itemCount_` is 0, the loop runs but just clears and skips every row. The result is a blank white space between the title and footer.

The footer still shows position counter logic (line 262-268):
```cpp
if (itemCount_ > 0) {
    snprintf(positionStr, sizeof(positionStr), "%u/%u  ", selection_ + 1, itemCount_);
    prefix = positionStr;
}
```

So the footer shows "  [hint]" with an empty prefix when the list is empty.

## Recommended Fix
Add an empty state rendering branch in `ListView::render()` that displays:
1. A centered "(empty)" or "No items" message in the list area
2. Optionally, a small icon or visual indicator
3. The existing footer hint remains to guide navigation

Example implementation in `ListView.cpp` around line 212 (after clearing item area):
```cpp
// Empty state
if (itemCount_ == 0) {
    const char* emptyText = tr(StringId::EMPTY);  // or add new string
    int textWidth = 0, textHeight = 0;
    gfx->getTextBounds(emptyText, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int textX = (width - textWidth) / 2;
    int textY = LIST_START_Y + (visibleItems_ * itemHeight_) / 2 - textHeight / 2;
    gfx->setCursor(textX, textY);
    gfx->print(emptyText);
    continue;  // Skip to next iteration for scroll indicator
}
```

Add translation string in `cdc_ui/I18n.h`:
```cpp
STR_EMPTY = "No items",
STR_EMPTY_DE = "Keine Eintraege",
```

## References
- Material Design Empty States: https://material.io/design/components/lists.html#empty-states
- iOS Human Interface Guidelines - Empty States: https://developer.apple.com/design/human-interface-guidelines/looking-at-data/
