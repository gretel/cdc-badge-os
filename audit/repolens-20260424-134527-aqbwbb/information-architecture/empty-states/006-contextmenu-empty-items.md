---
title: "[LOW] ContextMenuView renders empty box when no items provided"
severity: LOW
domain: ui-framework
lens: empty-states
labels:
  - "empty-state"
  - "contextmenu"
  - "popup"
---

## Summary
The `ContextMenuView` component in `components/cdc_views/src/ContextMenuView.cpp` renders a popup box with just a title when `itemCount_` is 0. No items are displayed and no message indicates why the menu is empty.

**Location:** `components/cdc_views/src/ContextMenuView.cpp:188-212` (render function)

## Impact
When a context menu is shown with no items (due to dynamic filtering or empty action list), users see:
- A popup box with a title but no content
- No explanation of why no actions are available
- Confusion about what to do next

## Evidence
In `ContextMenuView::render()` (lines 188-212):
```cpp
// Draw items
int itemY = boxY + TITLE_HEIGHT + BOX_PADDING;
for (uint8_t i = 0; i < visibleCount; i++) {
    uint8_t itemIndex = scrollPos_ + i;
    if (itemIndex >= itemCount_) break;

    const ContextMenuItem& item = items_[itemIndex];
    // ... renders item ...
}
```

When `itemCount_` is 0:
- `visibleCount` becomes 0 (line 139: `uint8_t visibleCount = std::min(itemCount_, VISIBLE_ITEMS);`)
- The loop doesn't execute
- An empty box is shown with just the title

The `navigate()` function (line 52-53) just returns early:
```cpp
void ContextMenuView::navigate(bool down) {
    if (itemCount_ == 0) return;  // <-- No feedback to user
```

## Recommended Fix
Add empty state text rendering in `ContextMenuView::render()`:

```cpp
// Draw items
int itemY = boxY + TITLE_HEIGHT + BOX_PADDING;
if (itemCount_ == 0) {
    // Empty state
    const char* emptyText = tr(StringId::EMPTY);
    int16_t x1, y1, textWidth, textHeight;
    gfx->getTextBounds(emptyText, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int textX = boxX + (boxWidth - textWidth) / 2;
    int textY = itemY + (ITEM_HEIGHT - textHeight) / 2;
    gfx->setCursor(textX, textY);
    gfx->print(emptyText);
} else {
    for (uint8_t i = 0; i < visibleCount; i++) {
        // ... existing item rendering ...
    }
}
```

Alternatively, prevent showing the menu when empty by checking in `showContextMenu()`:
```cpp
ContextMenuView* showContextMenu(const char* title, const ContextMenuItem* items, uint8_t count) {
    if (count == 0) return nullptr;  // Don't show empty menu
    s_sharedContextMenu.init(title, items, count);
    ViewStack::instance().showModal(&s_sharedContextMenu);
    return &s_sharedContextMenu;
}
```

## References
- ContextMenuView: `components/cdc_views/include/cdc_views/ContextMenuView.h`
- ContextMenuView implementation: `components/cdc_views/src/ContextMenuView.cpp`
