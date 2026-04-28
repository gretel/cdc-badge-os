---
title: "[LOW] Duplicate icon rendering code across MessageBox, ToastView, and ConfirmView"
severity: LOW
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
Similar icon rendering code is duplicated across MessageBox, ToastView, and ConfirmView components. Each component implements its own versions of checkmark, X mark, warning triangle, and info circle icons with slight variations instead of sharing a common icon rendering library.

**Files affected:**
- `components/cdc_views/src/MessageBox.cpp:140-175` (icon rendering switch)
- `components/cdc_views/src/ToastView.cpp:106-148` (icon rendering switch)
- `components/cdc_views/src/ConfirmView.cpp:94-118` (icon rendering switch)

## Impact
- **Code duplication**: Same icon shapes drawn multiple times with slight variations
- **Maintenance overhead**: Bug fixes or style changes must be applied in multiple places
- **Inconsistent icons**: Slight variations accumulate over time (different sizes, stroke weights)
- **Bundle size**: Each component carries its own icon rendering code

## Evidence

### Checkmark Icon - Three Implementations

**MessageBox (16px, single stroke + extra for thickness):**
```cpp
// components/cdc_views/src/MessageBox.cpp:146-149
gfx->drawLine(iconX + 2, iconY + 8, iconX + 6, iconY + 12, EPD_BLACK);
gfx->drawLine(iconX + 6, iconY + 12, iconX + 14, iconY + 4, EPD_BLACK);
gfx->drawLine(iconX + 2, iconY + 9, iconX + 6, iconY + 13, EPD_BLACK);
gfx->drawLine(iconX + 6, iconY + 13, iconX + 14, iconY + 5, EPD_BLACK);
```

**ToastView (12px, double stroke):**
```cpp
// components/cdc_views/src/ToastView.cpp:109-113
gfx->drawLine(iconX - 5, iconY, iconX - 2, iconY + 4, EPD_BLACK);
gfx->drawLine(iconX - 2, iconY + 4, iconX + 6, iconY - 5, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY + 1, iconX - 2, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 2, iconY + 5, iconX + 6, iconY - 4, EPD_BLACK);
```

**ToastView TASK (hourglass with filled triangles - unique):**
```cpp
// components/cdc_views/src/ToastView.cpp:133-139
gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY - 6, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY + 6, iconX + 5, iconY + 6, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY + 6, EPD_BLACK);
gfx->drawLine(iconX + 5, iconY - 6, iconX - 5, iconY + 6, EPD_BLACK);
gfx->fillTriangle(iconX - 3, iconY - 4, iconX + 3, iconY - 4, iconX, iconY - 1, EPD_BLACK);
gfx->fillTriangle(iconX - 3, iconY + 4, iconX + 3, iconY + 4, iconX, iconY + 1, EPD_BLACK);
```

### X Mark Icon - Three Implementations

**MessageBox:**
```cpp
// components/cdc_views/src/MessageBox.cpp:154-157
gfx->drawLine(iconX + 2, iconY + 2, iconX + 14, iconY + 14, EPD_BLACK);
gfx->drawLine(iconX + 14, iconY + 2, iconX + 2, iconY + 14, EPD_BLACK);
gfx->drawLine(iconX + 3, iconY + 2, iconX + 14, iconY + 13, EPD_BLACK);
gfx->drawLine(iconX + 13, iconY + 2, iconX + 2, iconY + 13, EPD_BLACK);
```

**ToastView:**
```cpp
// components/cdc_views/src/ToastView.cpp:118-122
gfx->drawLine(iconX - 5, iconY - 5, iconX + 5, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 5, iconY + 5, iconX + 5, iconY - 5, EPD_BLACK);
gfx->drawLine(iconX - 4, iconY - 5, iconX + 6, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 4, iconY + 5, iconX + 6, iconY - 5, EPD_BLACK);
```

**ConfirmView:**
```cpp
// components/cdc_views/src/ConfirmView.cpp:111-113
gfx->drawCircle(iconX, iconY, 8, EPD_BLACK);
gfx->drawLine(iconX - 4, iconY - 4, iconX + 4, iconY + 4, EPD_BLACK);
gfx->drawLine(iconX - 4, iconY + 4, iconX + 4, iconY - 4, EPD_BLACK);
```

### Warning Triangle - Two Implementations

**MessageBox:**
```cpp
// components/cdc_views/src/MessageBox.cpp:169-174
gfx->drawTriangle(
    iconX + 8, iconY + 1,
    iconX + 1, iconY + 14,
    iconX + 15, iconY + 14,
    EPD_BLACK
);
gfx->fillRect(iconX + 7, iconY + 5, 2, 5, EPD_BLACK);
gfx->fillRect(iconX + 7, iconY + 11, 2, 2, EPD_BLACK);
```

**ToastView:**
```cpp
// components/cdc_views/src/ToastView.cpp:142-145
gfx->drawTriangle(iconX, iconY - 7, iconX - 6, iconY + 6, iconX + 6, iconY + 6, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY - 2, 2, 5, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY + 4, 2, 2, EPD_BLACK);
```

**ConfirmView:**
```cpp
// components/cdc_views/src/ConfirmView.cpp:104-106
gfx->drawTriangle(iconX, iconY - 7, iconX - 7, iconY + 6, iconX + 7, iconY + 6, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY - 2, 2, 5, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY + 4, 2, 2, EPD_BLACK);
```

### Info Circle - Two Implementations

**MessageBox:**
```cpp
// components/cdc_views/src/MessageBox.cpp:162-164
gfx->drawCircle(iconX + 8, iconY + 8, 7, EPD_BLACK);
gfx->fillRect(iconX + 7, iconY + 4, 2, 2, EPD_BLACK);  // Dot
gfx->fillRect(iconX + 7, iconY + 7, 2, 5, EPD_BLACK);  // Stem
```

**ToastView:**
```cpp
// components/cdc_views/src/ToastView.cpp:127-130
gfx->drawCircle(iconX, iconY, 6, EPD_BLACK);
gfx->fillRect(iconX - 1, iconY - 3, 2, 2, EPD_BLACK);  // Dot
gfx->fillRect(iconX - 1, iconY, 2, 5, EPD_BLACK);       // Stem
```

## Recommended Fix

**Create a shared icon rendering library:**

1. **Create `components/cdc_views/src/IconRenderer.cpp` and `include/cdc_views/IconRenderer.h`:**
   ```cpp
   class IconRenderer {
   public:
       static void drawCheckmark(Gdey029T94* gfx, int x, int y, int size);
       static void drawX(Gdey029T94* gfx, int x, int y, int size);
       static void drawInfo(Gdey029T94* gfx, int x, int y, int size);
       static void drawWarning(Gdey029T94* gfx, int x, int y, int size);
       static void drawQuestion(Gdey029T94* gfx, int x, int y, int size);
       static void drawHourglass(Gdey029T94* gfx, int x, int y, int size);
   };
   ```

2. **Move existing implementations** to the shared library (choose one as canonical)

3. **Update MessageBox.cpp:**
   ```cpp
   #include "cdc_views/IconRenderer.h"
   // Replace inline drawing with:
   IconRenderer::drawCheckmark(gfx, iconX, iconY, 16);
   ```

4. **Update ToastView.cpp:**
   ```cpp
   #include "cdc_views/IconRenderer.h"
   // Replace inline drawing with:
   IconRenderer::drawCheckmark(gfx, iconX, iconY, 12);
   ```

5. **Update ConfirmView.cpp:**
   ```cpp
   #include "cdc_views/IconRenderer.h"
   // Replace inline drawing with:
   IconRenderer::drawQuestion(gfx, iconX, iconY, 16);
   ```

**Benefits:**
- Single source of truth for each icon shape
- Easy to update icon style globally
- Reduced code duplication (~100 lines saved)
- Consistent icon appearance across all components

## References
- DRY principle: Don't Repeat Yourself
- Current code has ~30% duplication in icon rendering logic
