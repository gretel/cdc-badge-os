---
title: "[LOW] No icon sizing constants or scale system for consistent icon dimensions"
severity: LOW
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
Icon sizes are hardcoded as magic numbers scattered throughout the codebase with no centralized sizing constants or scale system. This makes it difficult to maintain consistent icon sizes and adjust them globally if needed.

**Files affected:**
- `components/cdc_views/src/MessageBox.cpp:22` (ICON_SIZE = 16)
- `components/cdc_views/src/ConfirmView.cpp:91-92` (iconX, iconY with magic numbers)
- `components/cdc_views/src/ToastView.cpp:106-107` (iconX, iconY with magic numbers)
- `components/cdc_os_ui/src/views/LockScreenView.cpp:456` (iconSpacing = 14)

## Impact
- **Inconsistent sizing**: Icons appear at different sizes without clear rationale
- **Hard to maintain**: Changing icon size requires finding and updating multiple magic numbers
- **No design system**: Cannot easily implement responsive or scalable icon sizes
- **Visual drift**: Future changes may introduce more inconsistencies

## Evidence

### Hardcoded Icon Sizes

**MessageBox:**
```cpp
// components/cdc_views/src/MessageBox.cpp:22
static constexpr int ICON_SIZE = 16;
static constexpr int ICON_MARGIN = 8;
```

**ConfirmView (no constants, magic numbers used):**
```cpp
// components/cdc_views/src/ConfirmView.cpp:91-98
int iconX = boxX + 20;
int iconY = boxY + 22;
gfx->drawCircle(iconX, iconY, 8, EPD_BLACK);  // radius 8 = 16px diameter
```

**ToastView (no constants, magic numbers used):**
```cpp
// components/cdc_views/src/ToastView.cpp:106-107
int iconX = boxX + 20;
int iconY = boxY + (BOX_HEIGHT / 2);
gfx->drawCircle(iconX, iconY, 6, EPD_BLACK);  // radius 6 = 12px diameter
```

**LockScreen (spacing constant but no size constants):**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:456
const int iconSpacing = 14;
// Icon sizes are magic numbers in drawing code:
gfx->drawRect(iconX, y + 4, 8, 6, EPD_BLACK);  // 8x6px
```

### Inconsistent Icon Diameters

| Component | Icon Type | Size |
|-----------|-----------|------|
| MessageBox | All | 16px (ICON_SIZE) |
| ConfirmView | Question, Error | 16px (circle radius 8) |
| ConfirmView | Warning | ~13px (triangle height) |
| ToastView | All | ~12px (circle radius 6) |
| LockScreen | Status | 8-10px (varies by icon) |

## Recommended Fix

**Create a centralized icon sizing system:**

1. **Add icon sizing constants** in a shared header (e.g., `components/cdc_views/include/cdc_views/IconSizes.h`):
   ```cpp
   namespace icon {
       // Standard icon sizes
       static constexpr int SIZE_SMALL = 12;   // For compact lists, status bars
       static constexpr int SIZE_MEDIUM = 16;  // For dialogs, message boxes
       static constexpr int SIZE_LARGE = 20;   // For emphasis, main views
       
       // Spacing
       static constexpr int MARGIN = 8;        // Space around icons
       static constexpr int SPACING = 14;      // Space between status icons
       
       // Stroke weights
       static constexpr int STROKE_THIN = 1;
       static constexpr int STROKE_MEDIUM = 2;
   }
   ```

2. **Update MessageBox.cpp:**
   ```cpp
   #include "cdc_views/IconSizes.h"
   static constexpr int ICON_SIZE = icon::SIZE_MEDIUM;
   ```

3. **Update ConfirmView.cpp:**
   ```cpp
   #include "cdc_views/IconSizes.h"
   int iconX = boxX + 20;
   int iconY = boxY + 22;
   gfx->drawCircle(iconX, iconY, icon::SIZE_MEDIUM / 2, EPD_BLACK);
   ```

4. **Update ToastView.cpp:**
   ```cpp
   #include "cdc_views/IconSizes.h"
   // Change from radius 6 to use SIZE_SMALL
   gfx->drawCircle(iconX, iconY, icon::SIZE_SMALL / 2, EPD_BLACK);
   ```

5. **Update LockScreenView.cpp:**
   ```cpp
   #include "cdc_views/IconSizes.h"
   const int iconSpacing = icon::SPACING;
   ```

**Benefits:**
- Single source of truth for icon sizes
- Easy to adjust sizes globally
- Clear documentation of design decisions
- Enables consistent visual language

## References
- Material Design uses 24dp base with scale factors for different contexts
- E-Paper display constraint: 296x128 pixels limits available icon sizes
