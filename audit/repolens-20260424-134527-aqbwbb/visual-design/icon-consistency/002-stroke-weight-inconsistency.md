---
title: "[LOW] Inconsistent stroke weights for similar icons (checkmark, X, triangle)"
severity: LOW
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
Similar icon types (checkmark, X mark, warning triangle) are drawn with different stroke weights across components. Some use single-line strokes while others use double-lines to simulate thicker strokes, creating visual inconsistency when icons appear on the same screen.

**Files affected:**
- `components/cdc_views/src/MessageBox.cpp:141-157` (single-stroke checkmark, X, triangle)
- `components/cdc_views/src/ToastView.cpp:109-145` (double-stroke checkmark, X, triangle)
- `components/cdc_views/src/ConfirmView.cpp:94-115` (single-stroke icons)

## Impact
- **Visual inconsistency**: When MessageBox and ToastView icons appear together (e.g., in a list or sequence), they have noticeably different visual weights
- **Design cohesion**: Thicker icons appear "heavier" and more prominent than thinner ones at the same size
- **User experience**: Inconsistent icon weights can make the UI feel unpolished

## Evidence

### Checkmark Icon - Different Stroke Weights

**MessageBox (single stroke, 1px weight):**
```cpp
// components/cdc_views/src/MessageBox.cpp:141-143
gfx->drawLine(iconX + 2, iconY + 8, iconX + 6, iconY + 12, EPD_BLACK);
gfx->drawLine(iconX + 6, iconY + 12, iconX + 14, iconY + 4, EPD_BLACK);
```

**ToastView (double stroke, 2px weight):**
```cpp
// components/cdc_views/src/ToastView.cpp:109-113
gfx->drawLine(iconX - 5, iconY, iconX - 2, iconY + 4, EPD_BLACK);
gfx->drawLine(iconX - 2, iconY + 4, iconX + 6, iconY - 5, EPD_BLACK);
// Thicker
gfx->drawLine(iconX - 5, iconY + 1, iconX - 2, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 2, iconY + 5, iconX + 6, iconY - 4, EPD_BLACK);
```

### X Mark Icon - Different Stroke Weights

**MessageBox (single stroke):**
```cpp
// components/cdc_views/src/MessageBox.cpp:147-148
gfx->drawLine(iconX + 2, iconY + 2, iconX + 14, iconY + 14, EPD_BLACK);
gfx->drawLine(iconX + 14, iconY + 2, iconX + 2, iconY + 14, EPD_BLACK);
// Extra thickness
gfx->drawLine(iconX + 3, iconY + 2, iconX + 14, iconY + 13, EPD_BLACK);
gfx->drawLine(iconX + 13, iconY + 2, iconX + 2, iconY + 13, EPD_BLACK);
```

**ToastView (double stroke):**
```cpp
// components/cdc_views/src/ToastView.cpp:116-119
gfx->drawLine(iconX - 5, iconY - 5, iconX + 5, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY + 5, iconX + 5, iconY - 5, EPD_BLACK);
// Thicker
gfx->drawLine(iconX - 4, iconY - 5, iconX + 6, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 4, iconY + 5, iconX + 6, iconY - 5, EPD_BLACK);
```

### Warning Triangle - Different Stroke Weights

**MessageBox (single stroke):**
```cpp
// components/cdc_views/src/MessageBox.cpp:154-158
gfx->drawTriangle(
    iconX + 8, iconY + 1,
    iconX + 1, iconY + 14,
    iconX + 15, iconY + 14,
    EPD_BLACK
);
gfx->fillRect(iconX + 7, iconY + 5, 2, 5, EPD_BLACK);  // Line
gfx->fillRect(iconX + 7, iconY + 11, 2, 2, EPD_BLACK); // Dot
```

**ToastView (single stroke, different proportions):**
```cpp
// components/cdc_views/src/ToastView.cpp:142-145
gfx->drawTriangle(iconX, iconY - 7, iconX - 6, iconY + 6, iconX + 6, iconY + 6, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY - 2, 2, 5, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY + 4, 2, 2, EPD_BLACK);
```

## Recommended Fix

Choose a **consistent stroke weight strategy** for all icons:

**Option A: Single-stroke with standard line width**
- All icons use `drawLine()` with single lines (1px weight)
- Update ToastView to match MessageBox style (simpler, cleaner)

**Option B: Double-stroke for visual weight**
- All icons use double lines for 2px weight appearance
- Update MessageBox and ConfirmView to match ToastView style

**Option C: Fill-based icons**
- Use `fillRect()` and `fillTriangle()` for solid icons
- Provides most consistent visual weight across all icons

**Implementation steps:**
1. Decide on stroke strategy (A, B, or C)
2. Update ToastView.cpp or MessageBox.cpp to match chosen strategy
3. Ensure ConfirmView uses same stroke approach
4. Add comment in code documenting the stroke weight convention

## References
- Material Design: Uses consistent 2px stroke for outlined icons, filled icons for solid style
- E-Paper display consideration: Single-stroke icons may appear too thin at small sizes (12px)
