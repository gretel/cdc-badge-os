---
title: "[LOW] Border-radius values scattered without coherent scale"
severity: LOW
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary

The codebase uses various border-radius values for dialog frames and boxes, but these values don't follow a coherent scale or use consistent tokens.

**Files affected:**
- `components/cdc_views/src/RenderHelpers.cpp:147-154`
- `components/cdc_views/include/cdc_views/RenderHelpers.h`
- `components/cdc_views/src/MessageBox.cpp`
- `components/cdc_views/src/ToastView.cpp`
- `components/cdc_views/src/ConfirmView.cpp`

## Impact

1. **Visual inconsistency**: Different rounded corner sizes may appear across components
2. **No design token system**: Border-radius values are hardcoded, making global changes difficult
3. **Hard to maintain design consistency**: Adding new rounded components requires guessing appropriate values

## Evidence

### RenderHelpers.cpp:147-154
```cpp
void drawDialogFrame(Gdey029T94* gfx, int x, int y, int w, int h) {
    if (!gfx) return;

    gfx->fillRect(x, y, w, h, EPD_WHITE);
    gfx->drawRect(x, y, w, h, EPD_BLACK);
    gfx->drawRect(x + 1, y + 1, w - 2, h - 2, EPD_BLACK);  // Double border
}
```

Note: The current implementation uses **sharp corners** (rectangles), not rounded corners.

### Current border-radius usage in the codebase:

Searching for `drawCircle`, `fillCircle`, or radius-based drawing:

**MessageBox.cpp:155-175** (Icon rendering, not boxes):
```cpp
case MessageIcon::INFO:
    gfx->drawCircle(iconX + 8, iconY + 8, 7, EPD_BLACK);  // Icon, not box
    break;

case MessageIcon::WARNING:
    gfx->drawTriangle(...);  // Triangle, not rounded
    break;
```

**LockScreenView.cpp:462** (Lock icon):
```cpp
gfx->drawCircle(iconX + 4, y + 3, 3, EPD_BLACK);  // Icon detail
```

**LockScreenView.cpp:465-477** (WiFi icon):
```cpp
gfx->fillCircle(cx, cy, 1, EPD_BLACK);  // Base dot
gfx->drawLine(cx - 2, cy - 3, cx, cy - 4, EPD_BLACK);  // Arcs (not circles)
```

**LockScreenView.cpp:514** (Backlight/sun icon):
```cpp
gfx->fillCircle(iconX + 4, y + 5, 2, EPD_BLACK);  // Sun center
```

### Key observation:

The current codebase **does not use border-radius for boxes**. All dialog frames use sharp corners (rectangles). The circle drawing is used for:
- Icon details (INFO icon circle, WiFi arcs, sun center)
- Lock icon shackle
- PIN entry dots (PinEntryView.cpp:264-270)

### PIN Entry dots (PinEntryView.cpp:264-270):
```cpp
if (i < length_) {
    // Filled dot for entered digits
    gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
} else {
    // Empty dot for remaining positions
    gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
}
```

Radius: `PIN_DOT_SIZE / 2 - 1` = `16 / 2 - 1` = `7` pixels

## Recommended Fix

### Option 1: Add border-radius tokens for future use

Even though current dialogs use sharp corners, establish tokens for when rounded corners are desired:

```cpp
// Spacing.h or new BorderRadius.h
namespace radius {
    constexpr uint8_t none = 0;      // Sharp corners (current default)
    constexpr uint8_t sm = 2;        // Subtle round
    constexpr uint8_t md = 4;        // Medium round
    constexpr uint8_t lg = 8;        // Large round
    constexpr uint8_t full = 999;    // Pill shape
}
```

### Option 2: Create a drawRoundedRect helper

Add to RenderHelpers:

```cpp
// RenderHelpers.h
void drawRoundedRect(Gdey029T94* gfx, int x, int y, int w, int h, uint8_t radius);
void fillRoundedRect(Gdey029T94* gfx, int x, int y, int w, int h, uint8_t radius);
```

```cpp
// RenderHelpers.cpp
void drawRoundedRect(Gdey029T94* gfx, int x, int y, int w, int h, uint8_t radius) {
    if (radius == 0) {
        gfx->drawRect(x, y, w, h, EPD_BLACK);
        return;
    }
    
    // Draw rounded rectangle using arcs at corners
    gfx->drawLine(x + radius, y, x + w - radius, y, EPD_BLACK);  // Top
    gfx->drawLine(x + radius, y + h, x + w - radius, y + h, EPD_BLACK);  // Bottom
    gfx->drawLine(x, y + radius, x, y + h - radius, EPD_BLACK);  // Left
    gfx->drawLine(x + w, y + radius, x + w, y + h - radius, EPD_BLACK);  // Right
    
    gfx->drawCircle(x + radius, y + radius, radius, EPD_BLACK);  // Top-left
    gfx->drawCircle(x + w - radius, y + radius, radius, EPD_BLACK);  // Top-right
    gfx->drawCircle(x + radius, y + h - radius, radius, EPD_BLACK);  // Bottom-left
    gfx->drawCircle(x + w - radius, y + h - radius, radius, EPD_BLACK);  // Bottom-right
}
```

### Option 3: Standardize PIN dot rendering

The PIN entry dots use `PIN_DOT_SIZE / 2 - 1` for radius. Consider using a dedicated token:

```cpp
// PinEntryView.cpp
static constexpr int PIN_DOT_RADIUS = 7;  // Or use spacing::sm - 1
```

### Recommended approach

**Option 1** is recommended as a first step:
1. Add radius tokens to Spacing.h (or new file)
2. Document that current dialogs use sharp corners by design
3. Keep tokens ready for when rounded corners are desired
4. PIN dots are intentionally circular, no change needed

## References

- Material Design shape system: https://m3.material.com/foundations/layout/understanding-shape
- Current design uses sharp corners for high contrast on e-paper display
