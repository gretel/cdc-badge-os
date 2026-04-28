---
title: "[LOW] Mixed filled and outlined shapes within single icons"
severity: LOW
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
Some icons use a mix of filled (`fillTriangle`, `fillRect`) and outlined (`drawTriangle`, `drawLine`) shapes within the same icon, creating inconsistent visual weight. The ToastView TASK (hourglass) icon uses filled triangles for the hourglass bodies but outlined lines for the frame, while the ALERT (warning) icon uses an outlined triangle with filled rectangles for the exclamation mark.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp:133-145` (TASK and ALERT icon rendering)
- `components/cdc_views/src/MessageBox.cpp:169-176` (WARNING icon rendering)

## Impact
- **Visual inconsistency**: Icons with mixed fill/stroke styles appear "patchy" and lack design cohesion
- **Optical weight variation**: Filled shapes appear heavier than outlined shapes at the same size
- **Design rationale unclear**: No documentation explains why some parts are filled and others are outlined

## Evidence

### ToastView TASK Icon - Mixed Styles
```cpp
// components/cdc_views/src/ToastView.cpp:133-139
// Outlined frame
gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY - 6, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY + 6, iconX + 5, iconY + 6, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY + 6, EPD_BLACK);
gfx->drawLine(iconX + 5, iconY - 6, iconX - 5, iconY + 6, EPD_BLACK);
// Filled triangles (hourglass bodies)
gfx->fillTriangle(iconX - 3, iconY - 4, iconX + 3, iconY - 4, iconX, iconY - 1, EPD_BLACK);
gfx->fillTriangle(iconX - 3, iconY + 4, iconX + 3, iconY + 4, iconX, iconY + 1, EPD_BLACK);
```

**Visual result**: The hourglass has thin outlined lines for the frame but heavy filled triangles for the bodies, creating a "top-heavy" appearance.

### ToastView ALERT Icon - Mixed Styles
```cpp
// components/cdc_views/src/ToastView.cpp:142-145
// Outlined triangle
gfx->drawTriangle(iconX, iconY - 7, iconX - 6, iconY + 6, iconX + 6, iconY + 6, EPD_BLACK);
// Filled exclamation mark
gfx->fillRect(iconX - 1, iconY - 2, 2, 5, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY + 4, 2, 2, EPD_BLACK);
```

**Visual result**: The warning triangle outline is thin (1px) while the exclamation mark is thick (2px filled).

### MessageBox WARNING Icon - Similar Mixed Pattern
```cpp
// components/cdc_views/src/MessageBox.cpp:169-176
gfx->drawTriangle(
    iconX + 8, iconY + 1,
    iconX + 1, iconY + 14,
    iconX + 15, iconY + 14,
    EPD_BLACK
);
gfx->fillRect(iconX + 7, iconY + 5, 2, 5, EPD_BLACK);  // Line
gfx->fillRect(iconX + 7, iconY + 11, 2, 2, EPD_BLACK); // Dot
```

## Recommended Fix

**Choose a consistent style approach for mixed-shape icons:**

**Option A: All outlined**
- Replace `fillTriangle` with `drawTriangle` for TASK icon
- Keep `fillRect` for small details (exclamation dot/line) since they're too small to outline effectively

**Option B: All filled**
- Replace `drawLine` frame with filled shapes for TASK icon
- Keep `drawTriangle` for ALERT outline (or fill the entire triangle)

**Option C: Document the rationale**
- If mixed styles are intentional (e.g., filled for emphasis, outlined for structure), add code comments explaining the design decision
- Ensure the rationale applies consistently across all similar icons

**Recommended implementation (Option A with exceptions for small details):**

```cpp
// ToastView TASK icon - all outlined except small fill details
gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY - 6, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY + 6, iconX + 5, iconY + 6, EPD_BLACK);
gfx->drawTriangle(iconX - 5, iconY - 6, iconX + 5, iconY - 6, iconX, iconY - 1, EPD_BLACK);  // Top filled
gfx->drawTriangle(iconX - 5, iconY + 6, iconX + 5, iconY + 6, iconX, iconY + 1, EPD_BLACK);  // Bottom filled
```

## References
- Material Design: Uses consistent fill or outline style per icon category
- E-Paper display consideration: Small filled areas (2px wide) may appear as solid blocks regardless of style choice
