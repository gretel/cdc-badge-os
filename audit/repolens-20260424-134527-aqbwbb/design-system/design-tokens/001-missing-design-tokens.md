---
title: "[MEDIUM] Missing centralized design token system for UI styling"
severity: MEDIUM
domain: design-system
lens: design-tokens
labels:
  - "audit:design-system/design-tokens"
---

## Summary

The CDC Badge OS UI system lacks a centralized design token infrastructure. All styling values (spacing, colors, typography, dimensions) are hardcoded as local `static constexpr` values within each view component file, with no shared token layer.

**Affected Components:**
- `components/cdc_views/src/ListView.cpp:20-32` - Local constants: `TITLE_Y`, `LIST_START_Y`, `ITEM_PADDING_X`, `SCROLL_INDICATOR_WIDTH`, `VISIBLE_ITEMS`
- `components/cdc_views/src/SliderView.cpp:22-27` - Local constants: `TITLE_Y`, `VALUE_Y`, `BAR_Y`, `BAR_HEIGHT`, `BAR_MARGIN`
- `components/cdc_views/src/InfoView.cpp:21-26` - Local constants: `TITLE_Y`, `TEXT_START_Y`, `TEXT_MARGIN`, `FOOTER_HEIGHT`, `SCROLL_INDICATOR_WIDTH`
- `components/cdc_views/src/MessageBox.cpp:21-26` - Local constants: `BOX_PADDING`, `ICON_SIZE`, `ICON_MARGIN`, `MIN_BOX_WIDTH`, `MAX_BOX_WIDTH`
- `components/cdc_os_ui/src/views/LockScreenView.cpp:28-58` - Local constants: `CLOCK_Y`, `DATE_Y`, `ICONS_Y`, `NAME_Y`, `INFO_Y`, `INFO2_Y`, `BATTERY_X`, `BAT_WIDTH`, `BAT_HEIGHT`
- `components/cdc_views/src/RenderHelpers.h:9-10` - Only 2 shared constants: `kFooterHeight = 16`, `kScrollIndicatorWidth = 8`

## Impact

**Maintenance Burden:**
- Changing a global spacing value (e.g., footer height) requires editing multiple files
- Inconsistent spacing across views due to duplicated constants (`TEXT_MARGIN = 8` in InfoView vs `ITEM_PADDING_X = 10` in ListView)
- Hard to maintain visual consistency across the UI

**Design Iteration Speed:**
- Theme changes (e.g., switching from monochrome to grayscale) require manual updates in every view
- No easy way to create dark mode or alternative themes
- Typography changes require modifying each view individually

**Code Quality:**
- No single source of truth for design values
- Risk of "magic numbers" creeping in during quick fixes
- Difficult for new developers to understand the design system

## Evidence

**Hardcoded Spacing Values (repeated across files):**
```cpp
// ListView.cpp:20-25
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;

// InfoView.cpp:21-26
static constexpr int TITLE_Y = 5;
static constexpr int TEXT_START_Y = 28;
static constexpr int TEXT_MARGIN = 8;
static constexpr int FOOTER_HEIGHT = 16;

// SliderView.cpp:22-27
static constexpr int TITLE_Y = 20;
static constexpr int VALUE_Y = 55;
static constexpr int BAR_Y = 85;
static constexpr int BAR_HEIGHT = 20;
static constexpr int BAR_MARGIN = 20;
```

**Hardcoded Color Values:**
```cpp
// Used directly throughout all views
gfx->fillScreen(EPD_WHITE);
gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
gfx->setTextColor(EPD_BLACK);
```

**Hardcoded Font Sizes:**
```cpp
// LockScreenView.cpp:598-600
gfx->setTextSize(2);  // Size 2 for better fit
gfx->setCursor(5, CLOCK_Y);

// ListView.cpp:198
gfx->setTextSize(1);
```

**Minimal Shared Constants (RenderHelpers.h):**
```cpp
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;
```

## Recommended Fix

Create a centralized design token header file that defines all UI constants in a structured, organized manner:

**Step 1: Create `components/cdc_ui/include/cdc_ui/DesignTokens.h`**

```cpp
#pragma once

namespace cdc::ui::tokens {

// === Colors ===
constexpr uint16_t COLOR_BACKGROUND = EPD_WHITE;
constexpr uint16_t COLOR_FOREGROUND = EPD_BLACK;
constexpr uint16_t COLOR_SELECTED_BG = EPD_BLACK;
constexpr uint16_t COLOR_SELECTED_FG = EPD_WHITE;

// === Spacing ===
constexpr int SPACING_XS = 4;
constexpr int SPACING_SM = 8;
constexpr int SPACING_MD = 10;
constexpr int SPACING_LG = 12;
constexpr int SPACING_XL = 16;

// === Typography ===
constexpr int FONT_SIZE_SMALL = 1;   // Built-in 6x8
constexpr int FONT_SIZE_MEDIUM = 2;  // 9pt
constexpr int FONT_SIZE_LARGE = 3;   // 12pt
constexpr int FONT_SIZE_XLARGE = 4;  // 18pt
constexpr int FONT_SIZE_XXLARGE = 5; // 24pt

// === Layout ===
constexpr int FOOTER_HEIGHT = 16;
constexpr int HEADER_HEIGHT = 20;
constexpr int SCROLL_INDICATOR_WIDTH = 8;
constexpr int LIST_ITEM_HEIGHT = 18;

// === Borders & Shadows ===
constexpr int BORDER_WIDTH = 1;
constexpr int BORDER_RADIUS = 0;  // E-Paper is typically sharp edges

// === View-specific constants (organized by view) ===
namespace list {
    constexpr int TITLE_Y = 5;
    constexpr int LIST_START_Y = 30;
    constexpr int ITEM_PADDING_X = 10;
    constexpr uint8_t VISIBLE_ITEMS = 4;
}

namespace slider {
    constexpr int TITLE_Y = 20;
    constexpr int VALUE_Y = 55;
    constexpr int BAR_Y = 85;
    constexpr int BAR_HEIGHT = 20;
    constexpr int BAR_MARGIN = 20;
}

namespace info {
    constexpr int TITLE_Y = 5;
    constexpr int TEXT_START_Y = 28;
    constexpr int TEXT_MARGIN = 8;
}

namespace message {
    constexpr int BOX_PADDING = 12;
    constexpr int ICON_SIZE = 16;
    constexpr int ICON_MARGIN = 8;
    constexpr int MIN_BOX_WIDTH = 120;
    constexpr int MAX_BOX_WIDTH = 260;
}

namespace lockscreen {
    constexpr int CLOCK_Y = 5;
    constexpr int DATE_Y = 22;
    constexpr int ICONS_Y = 5;
    constexpr int NAME_Y = 60;
    constexpr int INFO_Y = 80;
    constexpr int INFO2_Y = 96;
    constexpr int BATTERY_X = 260;
    constexpr int BATTERY_Y = 5;
    constexpr int BAT_WIDTH = 28;
    constexpr int BAT_HEIGHT = 12;
}

} // namespace cdc::ui::tokens
```

**Step 2: Update one view as a migration example**

Modify `components/cdc_views/src/ListView.cpp` to use tokens:
```cpp
// Replace:
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;

// With:
#include "cdc_ui/DesignTokens.h"
using namespace cdc::ui::tokens;
using namespace cdc::ui::tokens::list;
```

## References

- **Design Tokens Best Practices**: https://designsystem.digital.gov/design-tokens/
- **CSS Custom Properties for Design Systems**: https://web.dev/articles/design-tokens
- **Style Dictionary (if migrating to build pipeline)**: https://amzn.github.io/style-dictionary/
- **Figma to Code with Design Tokens**: https://www.figma.com/plugin-docs/design-tokens/

## Migration Notes

This is a **medium-priority refactoring** that will:
1. Improve maintainability by having a single source of truth
2. Enable easier theme support in the future
3. Reduce duplication across view files
4. Make onboarding new developers easier

**Recommended approach:**
- Create the token file first (1-2 hours)
- Migrate one view at a time (15-20 min per view)
- Start with `RenderHelpers.cpp` as it's already the most centralized
- Then migrate `ListView`, `SliderView`, `InfoView`, `MessageBox`, `LockScreenView`

Total estimated effort: **3-4 hours** for complete migration.
