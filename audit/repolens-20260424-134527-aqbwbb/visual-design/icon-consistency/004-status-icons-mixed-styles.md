---
title: "[LOW] Status icons use mixed rendering styles (graphical, text, pixel-based)"
severity: LOW
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
The lock screen status icons in `LockScreenView` use inconsistent rendering approaches: some are graphical (battery, USB, BLE), some use text characters ("zzZ", "z" for sleep), and some use scattered pixel drawing (caffeinated/coffee cup). This creates visual inconsistency within a single icon set.

**Files affected:**
- `components/cdc_os_ui/src/views/LockScreenView.cpp:448-557` (renderStatusIcons function)

## Impact
- **Visual inconsistency**: Status icons have different visual styles and weights
- **Alignment issues**: Text-based icons ("zzZ") don't align well with graphical icons
- **Scalability**: Text icons depend on current font settings, graphical icons are fixed size
- **Maintenance**: Mixed approaches make it harder to add new icons consistently

## Evidence

### Mixed Rendering Approaches in Status Icons

**Graphical icon (Lock - padlock):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:458-463
gfx->drawRect(iconX, y + 4, 8, 6, EPD_BLACK);
gfx->drawCircle(iconX + 4, y + 3, 3, EPD_BLACK);
```

**Graphical icon (WiFi - arcs):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:466-483
int cx = iconX + 4;
int cy = iconX + 10;
gfx->fillCircle(cx, cy, 1, EPD_BLACK);
gfx->drawLine(cx - 2, cy - 3, cx, cy - 4, EPD_BLACK);
// ... more arc lines
```

**Text-based icon (Deep sleep):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:531-534
gfx->setCursor(iconX, y + 2);
gfx->print("zzZ");
```

**Text-based icon (Light sleep):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:535-538
gfx->setCursor(iconX, y + 2);
gfx->print("z");
```

**Pixel-based icon (Caffeinated/coffee cup):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:541-555
int cx = iconX, cy = y;
gfx->drawRect(cx, cy + 3, 8, 7, EPD_BLACK);
gfx->drawLine(cx + 8, cy + 4, cx + 10, cy + 4, EPD_BLACK);
// ... handle lines
gfx->drawPixel(cx + 2, cy + 1, EPD_BLACK);  // Steam pixels
gfx->drawPixel(cx + 3, cy, EPD_BLACK);
gfx->drawPixel(cx + 5, cy + 1, EPD_BLACK);
gfx->drawPixel(cx + 6, cy, EPD_BLACK);
```

**Graphical icon (USB trident):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:497-513
gfx->drawLine(ux + 5, uy + 4, ux + 5, uy + 12, EPD_BLACK);
gfx->drawLine(ux + 2, uy + 4, ux + 8, uy + 4, EPD_BLACK);
// ... more USB lines
```

### Inconsistent Icon Sizing

- Lock icon: 8x10px (rect + circle)
- WiFi icon: ~10x10px (arcs)
- BLE icon: 8x10px (lines)
- USB icon: 11x12px (trident)
- Backlight (sun): 8x10px (circle + pixels)
- Sleep (text): varies by font ("zzZ" ≈ 18x6px, "z" ≈ 6x6px)
- Caffeinated: 10x10px (rect + pixels)

## Recommended Fix

**Standardize on graphical icons with consistent dimensions:**

1. **Create a unified status icon renderer** with fixed 10x10px bounding box
2. **Replace text-based sleep icons** with graphical moon/stars icon:
   - Draw a crescent moon using arc/circle segments
   - Optionally add small stars as filled circles
3. **Replace pixel-based coffee cup** with cleaner line-based drawing:
   - Use `drawRect()` for cup body
   - Use `drawLine()` for handle
   - Use curved lines or circles for steam (not scattered pixels)
4. **Document icon dimensions** in code comments for consistency

**Implementation steps:**
1. Create `renderStatusIcon()` helper function with standardized icon dimensions
2. Add sleep icon as graphical moon (replace "zzZ" text)
3. Redraw coffee cup with consistent line weights
4. Ensure all icons fit within same bounding box (10x10px recommended)
5. Update icon spacing if needed for consistency

## References
- Current icon spacing is 14px - ensure new icons fit within this constraint
- E-Paper display: 296x128 pixels, total status icon area ~80px wide (6 icons max)
