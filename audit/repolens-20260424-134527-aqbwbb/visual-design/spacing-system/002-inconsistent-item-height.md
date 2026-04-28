---
title: "[MEDIUM] Inconsistent ITEM_HEIGHT values across list-based components"
severity: MEDIUM
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary
List-based components use different item heights, creating inconsistent visual density across similar UI elements.

- **ListView** (`components/cdc_views/src/ListView.cpp:21`): `itemHeight_ = 18` (default `DEFAULT_ITEM_HEIGHT = 18`)
- **ContextMenuView** (`components/cdc_views/src/ContextMenuView.cpp:23`): `ITEM_HEIGHT = 16`

Additional Y-position inconsistencies for similar elements:
- **ListView**: `TITLE_Y = 5`, `LIST_START_Y = 30`
- **InfoView**: `TITLE_Y = 5`, `TEXT_START_Y = 28`
- **T9InputView**: `TITLE_Y = 5`, `TEXT_Y = 50`
- **SliderView**: `TITLE_Y = 20`, `VALUE_Y = 55`
- **DateInputView**: `TITLE_Y = 20`, `DATE_Y = 60`
- **TimeInputView**: `TITLE_Y = 20`, `TIME_Y = 55`
- **PinEntryView**: `TITLE_Y = 15`, `PIN_Y = 50`
- **LockScreenView**: `CLOCK_Y = 5`, `DATE_Y = 22`

## Impact
- **Visual inconsistency**: Lists and menus have different densities, making the UI feel disjointed
- **User experience**: Users must adjust to different spacing patterns when navigating between views
- **Code maintainability**: Each view defines its own spacing constants without a shared system

## Evidence
```cpp
// ListView.cpp:21
static constexpr int ITEM_PADDING_X = 10;
// ListView.h:36
static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 18;

// ContextMenuView.cpp:23
static constexpr int ITEM_HEIGHT = 16;

// LockScreenView.cpp:29-30
static constexpr int CLOCK_Y = 5;
static constexpr int DATE_Y = 22;

// SliderView.cpp:22-23
static constexpr int TITLE_Y = 20;
static constexpr int VALUE_Y = 55;
```

## Recommended Fix
1. Establish a unified spacing scale for the 296x128 display:
   - Base spacing unit: 4px
   - Standard item height: 18px (already used by ListView)
   - Header Y positions: 5px (tight), 20px (prominent)
   - Content start Y: 28-30px (consistent with available header space)

2. Create centralized constants in `Spacing.h`:
```cpp
namespace cdc::ui {
namespace layout {
constexpr int kItemHeight = 18;
constexpr int kHeaderY_Tight = 5;
constexpr int kHeaderY_Prominent = 20;
constexpr int kContentStart = 28;
}
}
```

3. Update all views to use centralized constants where applicable
4. Document exceptions with clear rationale

## References
- Related to issue #001 (BOX_PADDING inconsistencies)
- Display resolution: 296x128 pixels (Good Display GDEY029T94)
