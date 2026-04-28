---
title: "[MEDIUM] Fragmented icon system with inconsistent enums, sizes, and stroke weights"
severity: MEDIUM
domain: visual-design
lens: icon-consistency
labels:
  - "audit:visual-design/icon-consistency"
---

## Summary
The icon system in CDC Badge OS is fragmented across multiple components with:
1. **Four separate icon enum definitions** (`ConfirmView::Icon`, `MessageIcon`, `ToastView::Icon`, `StatusIcon`) instead of a unified icon type system
2. **Inconsistent icon sizes**: MessageBox uses 16px, ConfirmView uses 16px diameter, ToastView uses ~12px, LockScreen uses 8x10px
3. **Inconsistent stroke weights**: ToastView SUCCESS/ERROR icons use double lines for "thicker" appearance while other icons use single strokes
4. **No centralized icon rendering**: Each view draws icons procedurally with duplicated logic

**Files affected:**
- `components/cdc_views/include/cdc_views/ConfirmView.h:20-24` (Icon enum)
- `components/cdc_views/include/cdc_views/MessageBox.h:11-17` (MessageIcon enum)
- `components/cdc_views/include/cdc_views/ToastView.h:21-28` (Icon enum)
- `components/cdc_os_ui/include/cdc_os_ui/views/LockScreenView.h:19-33` (StatusIcon enum)
- `components/cdc_views/src/MessageBox.cpp:22` (ICON_SIZE = 16)
- `components/cdc_views/src/ToastView.cpp:106-145` (Icon rendering with inconsistent sizes)
- `components/cdc_views/src/ConfirmView.cpp:94-115` (Icon rendering)
- `components/cdc_os_ui/src/views/LockScreenView.cpp:455-557` (Status icon rendering)

## Impact
- **Maintenance burden**: Adding a new icon type requires changes in multiple places
- **Visual inconsistency**: Icons appear at different scales and weights across the UI
- **Code duplication**: Similar icon drawing logic repeated in 4+ files
- **Bundle size**: No tree-shaking or centralized icon system

## Evidence

### Fragmented Icon Enums

**ConfirmView::Icon** (3 types):
```cpp
// components/cdc_views/include/cdc_views/ConfirmView.h:20-24
enum class Icon : uint8_t {
    NONE = 0,
    QUESTION,
    WARNING,
    ERROR
};
```

**MessageIcon** (5 types):
```cpp
// components/cdc_views/include/cdc_views/MessageBox.h:11-17
enum class MessageIcon : uint8_t {
    NONE = 0,       // No icon
    SUCCESS,        // Checkmark
    ERROR,          // X mark
    INFO,           // Info circle
    WARNING         // Warning triangle
};
```

**ToastView::Icon** (6 types):
```cpp
// components/cdc_views/include/cdc_views/ToastView.h:21-28
enum class Icon : uint8_t {
    NONE = 0,
    SUCCESS,
    ERROR,
    INFO,
    TASK,
    ALERT
};
```

### Inconsistent Icon Sizes

```cpp
// MessageBox: 16px
// components/cdc_views/src/MessageBox.cpp:22
static constexpr int ICON_SIZE = 16;

// ToastView: ~12px (radius 6, diameter ~12)
// components/cdc_views/src/ToastView.cpp:126
gfx->drawCircle(iconX, iconY, 6, EPD_BLACK);

// LockScreen: 8x10px with 14px spacing
// components/cdc_os_ui/src/views/LockScreenView.cpp:456
const int iconSpacing = 14;
gfx->drawRect(iconX, y + 4, 8, 6, EPD_BLACK);  // Lock icon 8x6
```

### Inconsistent Stroke Weights

**ToastView SUCCESS** uses double lines (thicker):
```cpp
// components/cdc_views/src/ToastView.cpp:109-113
gfx->drawLine(iconX - 5, iconY, iconX - 2, iconY + 4, EPD_BLACK);
gfx->drawLine(iconX - 2, iconY + 4, iconX + 6, iconY - 5, EPD_BLACK);
// Thicker
gfx->drawLine(iconX - 5, iconY + 1, iconX - 2, iconY + 5, EPD_BLACK);
gfx->drawLine(iconX - 2, iconY + 5, iconX + 6, iconY - 4, EPD_BLACK);
```

**MessageBox SUCCESS** uses single lines:
```cpp
// components/cdc_views/src/MessageBox.cpp:141-143
gfx->drawLine(iconX + 2, iconY + 8, iconX + 6, iconY + 12, EPD_BLACK);
gfx->drawLine(iconX + 6, iconY + 12, iconX + 14, iconY + 4, EPD_BLACK);
```

## Recommended Fix

**Phase 1 (this issue): Create a unified icon system**

1. **Create a centralized icon header** `components/cdc_views/include/cdc_views/Icons.h`:
   - Define a single `Icon` enum with all icon types (QUESTION, WARNING, ERROR, SUCCESS, INFO, TASK, ALERT, LOCK, BATTERY, etc.)
   - Define a single `IconSize` enum or constexpr values for consistent sizing (e.g., SMALL=12, MEDIUM=16, LARGE=20)
   - Declare icon rendering functions with consistent stroke weights

2. **Implement icon rendering functions** in `components/cdc_views/src/Icons.cpp`:
   ```cpp
   void drawIcon(Gdey029T94* gfx, Icon icon, int x, int y, IconSize size = IconSize::MEDIUM);
   void drawStatusIcon(Gdey029T94* gfx, StatusIcon icon, int x, int y);
   ```

3. **Update one component first** (e.g., MessageBox) to use the new system as a reference

**Phase 2 (related issues): Migrate existing components**
- Migrate ConfirmView to use unified Icons
- Migrate ToastView to use unified Icons  
- Migrate LockScreenView to use unified StatusIcons

## References
- Similar pattern: Material Design icon system uses unified icon set with size variants
- ESP32 E-Paper display constraints: 296x128 pixels, monochrome (black/white)
- Current project style: Procedural drawing with GFX library (lines, circles, rectangles)
