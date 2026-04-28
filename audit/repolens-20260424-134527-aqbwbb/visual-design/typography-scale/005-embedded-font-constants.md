---
title: "[LOW] Hardcoded font size values in embedded views lack centralized constants"
severity: LOW
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The embedded e-paper display views use hardcoded text size values (1, 2, 3) scattered across multiple files without centralized typography constants. This makes it difficult to maintain consistent visual hierarchy and adjust typography globally.

**Evidence locations:**
- `components/cdc_views/src/ListView.cpp:200` - `gfx->setTextSize(1)`
- `components/cdc_views/src/TimeInputView.cpp:193` - `gfx->setTextSize(3)` for time display
- `components/cdc_views/src/SliderView.cpp:155,168,192` - `setTextSize(1)` and `setTextSize(2)`
- `components/cdc_views/src/DateInputView.cpp` - `setTextSize(1)` and `setTextSize(2)`
- `components/grove_led/src/RgbInputView.cpp:203,217,243` - `setTextSize(1)` and `setTextSize(2)`
- `components/cdc_os_ui/src/views/LockScreenView.cpp:598-600` - `setTextSize(2)` and `setTextSize(1)`

## Impact
- **Maintenance**: Changing font sizes requires hunting through multiple files
- **Consistency**: Same semantic text (e.g., "title") may use different sizes across views
- **Visual hierarchy**: No clear documentation of what each size value represents

## Evidence
```cpp
// TimeInputView.cpp - Title uses size 1, time display uses size 3
gfx->setTextSize(1);  // Title
render::drawHeaderCentered(gfx, title_, TITLE_Y, width);

gfx->setTextSize(3);  // Main time display
gfx->print(timeStr);

// SliderView.cpp - Inconsistent pattern
gfx->setTextSize(1);  // Title
gfx->setTextSize(2);  // Value
gfx->setTextSize(1);  // Arrow labels

// LockScreenView.cpp - Uses FONT_SIZES array but mixed with setTextSize
gfx->setFont(nullptr);
gfx->setTextSize(2);  // Clock
gfx->setTextSize(1);  // Date
```

Note: The codebase uses Adafruit-GFX `setTextSize()` which is a multiplier (1=6x8 pixels, 2=12x16, 3=18x24 for built-in font), not point sizes like `FreeMonoBold12pt7b`.

## Recommended Fix
Define centralized typography constants in a shared header:

1. **Create `components/cdc_views/include/cdc_views/Typography.h`**:
```cpp
#pragma once

namespace cdc::ui {

/** \brief Typography scale constants for e-paper display. */
struct Typography {
    // Built-in font (6x8) size multipliers
    static constexpr uint8_t SIZE_TINY = 1;    // 6x8 px
    static constexpr uint8_t SIZE_SMALL = 2;   // 12x16 px
    static constexpr uint8_t SIZE_MEDIUM = 3;  // 18x24 px
    static constexpr uint8_t SIZE_LARGE = 4;   // 24x32 px
    
    // Semantic size aliases
    static constexpr uint8_t FOOTER = SIZE_SMALL;
    static constexpr uint8_t BODY = SIZE_SMALL;
    static constexpr uint8_t TITLE = SIZE_MEDIUM;
    static constexpr uint8_t HEADING = SIZE_LARGE;
};

} // namespace cdc::ui
```

2. **Update views to use constants**:
```cpp
#include "cdc_views/Typography.h"

// Before:
gfx->setTextSize(1);
gfx->setTextSize(2);

// After:
gfx->setTextSize(Typography::FOOTER);
gfx->setTextSize(Typography::TITLE);
```

## References
- [Adafruit-GFX setTextSize](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts)
- [Embedded typography best practices](https://www.nngroup.com/articles/typography-mobile/)
