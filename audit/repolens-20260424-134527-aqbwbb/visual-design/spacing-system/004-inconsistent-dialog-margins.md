---
title: "[LOW] MessageBox and ConfirmView use different dialog frame constants"
severity: LOW
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary

MessageBox and ConfirmView both render centered dialog boxes but use different approaches and constants for sizing and positioning, leading to potentially inconsistent visual appearance.

**Files affected:**
- `components/cdc_views/src/MessageBox.cpp:22-26`
- `components/cdc_views/src/ConfirmView.cpp` (uses BOX_WIDTH/BOX_HEIGHT from header)
- `components/cdc_views/include/cdc_views/MessageBox.h`
- `components/cdc_views/include/cdc_views/ConfirmView.h`

## Impact

1. **Visual inconsistency**: Dialog boxes may appear with different proportions
2. **Confusing API**: Developers may not know which dialog to use for consistent appearance
3. **Maintenance burden**: Changes to dialog styling require updates in multiple places

## Evidence

### MessageBox.cpp:22-26
```cpp
static constexpr int BOX_PADDING = 12;
static constexpr int ICON_SIZE = 16;
static constexpr int ICON_MARGIN = 8;
static constexpr int MIN_BOX_WIDTH = 120;
static constexpr int MAX_BOX_WIDTH = 260;
```

### MessageBox rendering (MessageBox.cpp:115-130)
```cpp
// Calculate box dimensions dynamically
uint16_t contentWidth = textWidth;
if (icon_ != MessageIcon::NONE) {
    contentWidth += ICON_SIZE + ICON_MARGIN;
}

uint16_t boxWidth = contentWidth + BOX_PADDING * 2;
if (boxWidth < MIN_BOX_WIDTH) boxWidth = MIN_BOX_WIDTH;
if (boxWidth > MAX_BOX_WIDTH) boxWidth = MAX_BOX_WIDTH;

uint16_t boxHeight = textHeight + BOX_PADDING * 2;
if (boxHeight < 40) boxHeight = 40;

// Center the box
int boxX = (screenWidth - boxWidth) / 2;
int boxY = (screenHeight - boxHeight) / 2;
```

### ConfirmView.h (from ConfirmView.cpp context)
The ConfirmView uses hardcoded BOX_WIDTH and BOX_HEIGHT constants (likely defined in header):

```cpp
// In ConfirmView.cpp:76-77
int boxX = (width - BOX_WIDTH) / 2;
int boxY = (height - BOX_HEIGHT) / 2;

// In ConfirmView.cpp:84-85
int textX = boxX + 15;  // Hardcoded padding!
int textY = boxY + 20;  // Hardcoded padding!
```

### Comparison of approaches:

| Aspect | MessageBox | ConfirmView |
|--------|------------|-------------|
| Width calculation | Dynamic (based on content) | Fixed (BOX_WIDTH) |
| Height calculation | Dynamic (based on content) | Fixed (BOX_HEIGHT) |
| Horizontal padding | BOX_PADDING = 12 | Hardcoded 15 |
| Vertical padding | BOX_PADDING = 12 | Hardcoded 20 |
| Icon margin | ICON_MARGIN = 8 | Not applicable |

## Recommended Fix

### Option 1: Unify dialog constants

Create shared constants in `RenderHelpers.h`:

```cpp
// RenderHelpers.h
namespace render {
    constexpr int dialogPadding = 12;
    constexpr int dialogMinWidth = 120;
    constexpr int dialogMaxWidth = 260;
    constexpr int dialogMinHeight = 40;
    constexpr int dialogIconSize = 16;
    constexpr int dialogIconMargin = 8;
}
```

### Option 2: Make ConfirmView dynamic like MessageBox

Update ConfirmView to calculate dimensions based on content:

```cpp
void ConfirmView::render(bool partial) {
    // ... get display ...
    
    // Calculate text bounds
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    gfx->getTextBounds(message_, 0, 0, &x1, &y1, &textWidth, &textHeight);
    
    // Calculate box dimensions (same as MessageBox)
    uint16_t boxWidth = textWidth + render::dialogPadding * 2;
    if (boxWidth < render::dialogMinWidth) boxWidth = render::dialogMinWidth;
    
    uint16_t boxHeight = textHeight + render::dialogPadding * 2;
    if (boxHeight < render::dialogMinHeight) boxHeight = render::dialogMinHeight;
    
    // Center and render...
}
```

### Option 3: Create a DialogView base class

For more complex dialogs, create a reusable base class:

```cpp
// DialogView.h
class DialogView : public ViewBase {
protected:
    void drawDialogFrame(Gdey029T94* gfx, int x, int y, int w, int h);
    void drawDialogContent(Gdey029T94* gfx, const char* message, Icon icon, int x, int y);
    
    int calculateBoxWidth(const char* message, Icon icon);
    int calculateBoxHeight(const char* message);
};
```

### Recommended approach

**Option 1** is simplest and provides immediate consistency:
1. Move constants to RenderHelpers.h
2. Update MessageBox.cpp to use `render::dialogPadding` instead of `BOX_PADDING`
3. Update ConfirmView.cpp to use the same padding values
4. Consider making ConfirmView dynamic if text varies significantly

## Testing

After fixing:
1. Test MessageBox with short/long messages
2. Test ConfirmView with short/long messages
3. Verify both dialogs look visually consistent
4. Check icon alignment in both dialogs

## References

- Related to issue 001 (centralized spacing system)
- MessageBox already has good dynamic sizing; ConfirmView should follow same pattern
