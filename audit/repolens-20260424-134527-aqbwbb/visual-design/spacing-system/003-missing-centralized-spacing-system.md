---
title: "[LOW] Missing centralized spacing system for display layout constants"
severity: LOW
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary
The CDC Badge OS UI system lacks a centralized spacing system. Each view component defines its own layout constants (Y positions, padding, margins, gaps) without sharing or referencing a common scale. This leads to:

1. **Magic number spacing**: Values like `13`, `17`, `22` appear without clear rationale
2. **Inconsistent naming**: `BOX_PADDING`, `TEXT_MARGIN`, `ITEM_PADDING_X`, `ICON_MARGIN` all serve similar purposes
3. **No spacing scale**: No defined relationship between spacing values (e.g., all should be multiples of a base unit)

### Current State
Each view file has its own local constants:
- `ListView.cpp`: `TITLE_Y = 5`, `LIST_START_Y = 30`, `ITEM_PADDING_X = 10`, `SCROLL_INDICATOR_WIDTH = 8`
- `LockScreenView.cpp`: `CLOCK_Y = 5`, `DATE_Y = 22`, `ICONS_Y = 5`, `NAME_Y = 60`, `INFO_Y = 80`, `INFO2_Y = 96`
- `SliderView.cpp`: `TITLE_Y = 20`, `VALUE_Y = 55`, `BAR_Y = 85`, `BAR_HEIGHT = 20`, `BAR_MARGIN = 20`
- `ContextMenuView.cpp`: `BOX_PADDING = 8`, `TITLE_HEIGHT = 18`, `ITEM_HEIGHT = 16`
- `MessageBox.cpp`: `BOX_PADDING = 12`, `ICON_SIZE = 16`, `ICON_MARGIN = 8`
- `InfoView.cpp`: `TITLE_Y = 5`, `TEXT_START_Y = 28`, `TEXT_MARGIN = 8`, `FOOTER_HEIGHT = 16`
- `PinEntryView.cpp`: `TITLE_Y = 15`, `PIN_Y = 50`, `PIN_DOT_SIZE = 16`, `PIN_DOT_SPACING = 24`, `RETRIES_Y = 80`
- `T9InputView.cpp`: `TITLE_Y = 5`, `TEXT_Y = 50`, `TEXT_MARGIN = 10`
- `DateInputView.cpp`: `TITLE_Y = 20`, `DATE_Y = 60`, `UNDERLINE_Y = DATE_Y + 20`, `HINT_Y = 90`
- `TimeInputView.cpp`: `TITLE_Y = 20`, `TIME_Y = 55`, `UNDERLINE_Y = TIME_Y + 25`, `HINT_Y = 90`
- `RgbInputView.cpp`: `TITLE_Y = 20`, `RGB_Y = 55`, `UNDERLINE_Y = RGB_Y + 20`, `PREVIEW_Y = 95`, `HINT_Y = 115`

## Impact
- **Maintenance burden**: Changes to spacing require updating multiple files
- **Visual drift**: New developers introduce inconsistent spacing without knowing the existing patterns
- **No design token system**: Cannot easily theme or adapt spacing for different display sizes

## Evidence
The following files each define their own layout constants without referencing a shared system:

```
components/cdc_views/src/ListView.cpp:21-24
components/cdc_views/src/SliderView.cpp:22-26
components/cdc_views/src/ContextMenuView.cpp:21-24
components/cdc_views/src/MessageBox.cpp:21-25
components/cdc_views/src/InfoView.cpp:21-25
components/cdc_views/src/PinEntryView.cpp:21-24
components/cdc_views/src/T9InputView.cpp:38-40
components/cdc_views/src/DateInputView.cpp:20-23
components/cdc_views/src/TimeInputView.cpp:20-23
components/cdc_views/src/RgbInputView.cpp:18-22
components/cdc_os_ui/src/views/LockScreenView.cpp:28-37
```

## Recommended Fix
Create a centralized spacing system header file:

1. **Create `components/cdc_views/include/cdc_views/Spacing.h`**:
```cpp
#pragma once
/**
 * \brief Centralized spacing constants for CDC Badge UI.
 *
 * All spacing values are based on a 4px grid system for the 296x128 display.
 */
namespace cdc::ui {
namespace spacing {

// Base units
constexpr int kUnit = 4;           // Base spacing unit
constexpr int kSm = 8;             // 2 units: tight spacing
constexpr int kMd = 10;            // 2.5 units: standard spacing
constexpr int kLg = 12;            // 3 units: generous spacing
constexpr int kXl = 16;            // 4 units: large spacing

// Layout dimensions
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;
constexpr int kDialogPadding = 12;
constexpr int kListPaddingX = 10;
constexpr int kItemHeight = 18;

// Title positions (use kHeaderY_Tight or kHeaderY_Prominent)
constexpr int kHeaderY_Tight = 5;
constexpr int kHeaderY_Prominent = 20;

// Content starts
constexpr int kContentStart = 28;

} // namespace spacing
} // namespace cdc::ui
```

2. **Update each view to use centralized constants**:
   - Replace local constants with `spacing::k...` references
   - Keep view-specific values local only when they truly need to differ

3. **Document the spacing scale** with usage guidelines and examples

## References
- Related to issue #001 (BOX_PADDING inconsistencies)
- Related to issue #002 (ITEM_HEIGHT inconsistencies)
- Display: Good Display GDEY029T94 (296x128 pixels, 1-bit e-ink)
