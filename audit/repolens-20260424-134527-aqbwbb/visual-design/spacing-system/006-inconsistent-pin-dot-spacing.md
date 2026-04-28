---
title: "[MEDIUM] Inconsistent PIN_DOT_SIZE and PIN_DOT_SPACING between PinEntryView and PinChangeView"
severity: MEDIUM
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary

Two PIN entry components use different values for PIN dot size and spacing, creating visual inconsistency in similar UI elements:

- **PinEntryView** (`components/cdc_views/src/PinEntryView.cpp:26-27`):
  - `PIN_DOT_SIZE = 16`
  - `PIN_DOT_SPACING = 24`

- **PinChangeView** (`components/cdc_os_ui/src/views/PinChangeView.cpp:25-26`):
  - `PIN_DOT_SIZE = 12`
  - `PIN_DOT_SPACING = 16`

This results in:
- PinEntryView dots: 16px diameter, 8px gap between dots
- PinChangeView dots: 12px diameter, 4px gap between dots

## Impact

1. **Visual inconsistency**: Users see different PIN entry styles when changing PIN vs. entering PIN
2. **Confusing UX**: The same action (entering a PIN) looks different depending on context
3. **Maintenance burden**: Two sets of constants to maintain for the same visual element
4. **Design coherence**: Breaks the expectation that similar UI elements should look consistent

## Evidence

### PinEntryView.cpp:26-27
```cpp
static constexpr int PIN_DOT_SIZE = 16;
static constexpr int PIN_DOT_SPACING = 24;
```

### PinChangeView.cpp:25-26
```cpp
static constexpr int PIN_DOT_SIZE = 12;
static constexpr int PIN_DOT_SPACING = 16;
```

### Rendering code comparison

**PinEntryView.cpp:260-268**
```cpp
int x = startX + i * PIN_DOT_SPACING;
if (i < length_) {
    gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
} else {
    gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
}
```

**PinChangeView.cpp:350-357**
```cpp
int x = startX + i * PIN_DOT_SPACING;
if (i < length_) {
    gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
} else {
    gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
}
```

The rendering logic is identical, but the constants differ.

## Recommended Fix

### Option 1: Use PinEntryView values (larger, more prominent)

Update PinChangeView to use the same constants as PinEntryView:

1. **Remove local constants from PinChangeView.cpp**
2. **Add include for PinEntryView constants** or create shared spacing tokens

```cpp
// PinChangeView.cpp - update constants section
// Remove:
// static constexpr int PIN_DOT_SIZE = 12;
// static constexpr int PIN_DOT_SPACING = 16;

// Add include and use shared constants:
#include "cdc_views/PinEntryView.h"  // Or create new Spacing.h

// Then use:
// PIN_DOT_SIZE = 16 (from shared)
// PIN_DOT_SPACING = 24 (from shared)
```

### Option 2: Create centralized spacing tokens

Create `components/cdc_views/include/cdc_views/Spacing.h`:

```cpp
#pragma once
namespace cdc::ui {
namespace spacing {
// PIN entry dots
constexpr int pinDotSize = 16;
constexpr int pinDotSpacing = 24;
constexpr int pinDotRadius = 7;  // pinDotSize / 2 - 1
}
}
```

Then update both files:

**PinEntryView.cpp**
```cpp
#include "cdc_views/Spacing.h"
static constexpr int PIN_DOT_SIZE = spacing::pinDotSize;
static constexpr int PIN_DOT_SPACING = spacing::pinDotSpacing;
```

**PinChangeView.cpp**
```cpp
#include "cdc_views/Spacing.h"
static constexpr int PIN_DOT_SIZE = spacing::pinDotSize;
static constexpr int PIN_DOT_SPACING = spacing::pinDotSpacing;
```

### Option 3: Smaller dots (if space is constrained)

If the larger dots don't fit well in PinChangeView's multi-step layout, adjust PinEntryView to match PinChangeView instead. Consider:
- Display width: 296px
- Max 4-digit PIN: 4 dots × 12px + 3 gaps × 4px = 60px total (fits easily)
- Centered: (296 - 60) / 2 = 118px margin on each side

## Testing

After fixing:
1. Test PIN entry on lock screen (PinEntryView)
2. Test PIN change wizard (PinChangeView)
3. Verify dots look identical in size and spacing
4. Ensure layout still fits within 296×128 display

## References

- Related to issue 001 (BOX_PADDING inconsistencies)
- Related to issue 003 (missing centralized spacing system)
- Display resolution: 296×128 pixels (Good Display GDEY029T94)

</content>