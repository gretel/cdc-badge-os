---
title: "[LOW] ContextMenuView and MessageBox share same rendering but no visual differentiation"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "modal-distinction"
  - "visual-hierarchy"
  - "dialog-types"
---

## Summary
The `ContextMenuView` (`components/cdc_views/src/ContextMenuView.cpp:170-210`) and `MessageBox` (`components/cdc_views/src/MessageBox.cpp:120-140`) both use the `drawDialogFrame()` helper to render their containers, resulting in visually identical modal dialogs. Users cannot immediately distinguish between a context menu (selection-based) and a message box (information-based).

## Impact
- **Modal confusion**: Users may not know if they need to select an item or just acknowledge a message
- **Interaction expectations**: Context menus expect selection; message boxes expect dismissal
- **Visual hierarchy**: Different types of modals should have different visual weight

## Evidence
File: `components/cdc_views/src/ContextMenuView.cpp`, lines 170-172

```cpp
// Draw box background
render::drawDialogFrame(gfx, boxX, boxY, boxWidth, boxHeight);
```

File: `components/cdc_views/src/MessageBox.cpp`, lines 120-122

```cpp
// Draw box background (white with black border)
render::drawDialogFrame(gfx, boxX, boxY, boxWidth, boxHeight);
```

Both use the exact same `drawDialogFrame()` call:
```cpp
void render::drawDialogFrame(Gdey029T94* gfx, int x, int y, int w, int h) {
    if (!gfx) return;
    gfx->fillRect(x, y, w, h, EPD_WHITE);
    gfx->drawRect(x, y, w, h, EPD_BLACK);
    gfx->drawRect(x + 1, y + 1, w - 2, h - 2, EPD_BLACK);
}
```

Both modals are rendered with:
- White background
- Double black border
- Same spacing and padding

## Recommended Fix
Differentiate the visual appearance of different modal types:

**Option A - Add a header bar to ContextMenuView**:
```cpp
// In ContextMenuView::render()
// Draw box with header bar
gfx->fillRect(boxX, boxY, boxWidth, boxHeight, EPD_WHITE);
gfx->drawRect(boxX, boxY, boxWidth, boxHeight, EPD_BLACK);
gfx->drawRect(boxX + 1, boxY + 1, boxWidth - 2, boxHeight - 2, EPD_BLACK);

// Add a header bar with title
gfx->fillRect(boxX + 2, boxY + 2, boxWidth - 4, TITLE_HEIGHT, EPD_BLACK);
gfx->setTextColor(EPD_WHITE);
gfx->setCursor(boxX + BOX_PADDING, boxY + 4);
gfx->print(title_);
```

**Option B - Add border style variation**:
```cpp
// Create new function for message boxes
void render::drawMessageBoxFrame(Gdey029T94* gfx, int x, int y, int w, int h) {
    gfx->fillRect(x, y, w, h, EPD_WHITE);
    gfx->drawRect(x, y, w, h, EPD_BLACK);
    // Single border for message boxes (lighter weight)
}

// Keep double border for context menus
void render::drawContextMenuFrame(Gdey029T94* gfx, int x, int y, int w, int h) {
    gfx->fillRect(x, y, w, h, EPD_WHITE);
    gfx->drawRect(x, y, w, h, EPD_BLACK);
    gfx->drawRect(x + 1, y + 1, w - 2, h - 2, EPD_BLACK);
    // Double border for context menus (heavier weight)
}
```

**Option C - Add icon emphasis to MessageBox**:
```cpp
// Make MessageBox icon more prominent
if (icon_ != MessageIcon::NONE) {
    // Draw icon with a circle background
    gfx->fillCircle(iconX + 8, iconY + 8, 10, EPD_BLACK);
    gfx->fillCircle(iconX + 8, iconY + 8, 8, EPD_WHITE);
    // Draw icon on top
    // ... existing icon drawing code ...
}
```

**Option D - Add footer hints to ContextMenuView**:
```cpp
// Show key hints at bottom of context menu
gfx->setTextSize(1);
gfx->setCursor(boxX + 5, boxY + boxHeight - 10);
gfx->print("[Y] ");
gfx->setTextColor(EPD_BLACK);
gfx->print("Select  ");
gfx->setTextColor(EPD_BLACK);
gfx->print("[N] ");
gfx->print("Close");
```

## References
- [Modal dialog patterns](https://www.w3.org/WAI/tutorials/modals/)
- [Visual hierarchy in UI design](https://www.nngroup.com/articles/visual-hierarchy/)
