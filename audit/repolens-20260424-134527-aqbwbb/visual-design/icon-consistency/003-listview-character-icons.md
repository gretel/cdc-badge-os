---
title: "[LOW] ListView uses character-based icons instead of graphical icons"
severity: LOW
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
The ListView component uses character-based icons (ASCII/text characters) drawn via `gfx->print(iconStr)` instead of graphical icons. This differs from the graphical icon system used in MessageBox, ToastView, and ConfirmView, creating an inconsistent visual language.

**Files affected:**
- `components/cdc_views/include/cdc_views/ListView.h:14` (icon as uint8_t)
- `components/cdc_views/src/ListView.cpp:235-244` (character-based rendering)

## Impact
- **Visual inconsistency**: Character icons (e.g., "?", "!", "✓") look different from graphical icons in other components
- **Limited styling**: Cannot control size, stroke weight, or position precisely
- **Font dependency**: Icon appearance depends on the current font, which may vary
- **Localization**: Special characters may not render correctly in all fonts

## Evidence

### Character-Based Icon Definition

```cpp
// components/cdc_views/include/cdc_views/ListView.h:14
struct ListItem {
    const char* label;          // Display text
    uint8_t icon = 0;           // Icon type (0 = none)
    bool iconDisabled = false;  // Draw icon crossed-out
    void* userData = nullptr;   // Optional user data
};
```

### Character-Based Rendering

```cpp
// components/cdc_views/src/ListView.cpp:235-244
if (item.icon) {
    char iconStr[2] = {static_cast<char>(item.icon), '\0'};
    gfx->setCursor(textX, y + 4);
    gfx->print(iconStr);
    if (item.iconDisabled) {
        uint16_t color = isSelected ? EPD_WHITE : EPD_BLACK;
        gfx->drawLine(textX, y + 10, textX + 6, y + 10, color);
    }
    textX += 10;
}
```

### Comparison with Graphical Icons

**MessageBox uses graphical icons:**
```cpp
// components/cdc_views/src/MessageBox.cpp:141-143
gfx->drawLine(iconX + 2, iconY + 8, iconX + 6, iconY + 12, EPD_BLACK);
gfx->drawLine(iconX + 6, iconY + 12, iconX + 14, iconY + 4, EPD_BLACK);
```

**ListView uses text characters:**
```cpp
// components/cdc_views/src/ListView.cpp:236-238
char iconStr[2] = {static_cast<char>(item.icon), '\0'};
gfx->print(iconStr);
```

## Recommended Fix

**Option A: Replace with graphical icons (recommended)**
1. Create a simple icon rendering function for list items
2. Define a mapping from icon types to graphical icon functions
3. Update ListView to draw icons procedurally instead of using text

**Option B: Use a dedicated icon font**
1. Create or use an icon font with consistent glyphs
2. Define a specific font for icons (e.g., 8x8 pixel icon font)
3. Set icon font before drawing, then reset to normal font

**Option C: Hybrid approach**
1. Keep character icons for simple cases (?, !, ✓, ✗)
2. Add an optional `iconRenderer` callback for custom graphical icons
3. Document the convention for when to use characters vs graphics

**Implementation steps (Option A):**
1. Add `IconType` enum to ListView.h (NONE, QUESTION, WARNING, SUCCESS, ERROR, INFO)
2. Create `renderListIcon()` function in ListView.cpp
3. Update `ListItem` struct to use `IconType` instead of `uint8_t`
4. Replace `gfx->print(iconStr)` with `renderListIcon()` call
5. Test with existing list views to ensure visual consistency

## References
- Current ListView usage should be searched to understand existing icon usage patterns
- Consider maintaining backward compatibility if existing code relies on character codes
