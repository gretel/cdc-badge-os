---
title: "[MEDIUM] Inconsistent BOX_PADDING values across modal dialog components"
severity: MEDIUM
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary
Multiple modal dialog components (MessageBox, ContextMenuView) define different `BOX_PADDING` constants, creating visual inconsistency in how content is spaced within similar UI elements.

- **MessageBox** (`components/cdc_views/src/MessageBox.cpp:22`): `BOX_PADDING = 12`
- **ContextMenuView** (`components/cdc_views/src/ContextMenuView.cpp:22`): `BOX_PADDING = 8`

Additional related inconsistencies:
- **Icon margins**: MessageBox uses `ICON_MARGIN = 8` (`MessageBox.cpp:23`), while ContextMenuView has no icon margin concept
- **Text margins**: InfoView uses `TEXT_MARGIN = 8` (`InfoView.cpp:23`), T9InputView uses `TEXT_MARGIN = 10` (`T9InputView.cpp:40`), ListView uses `ITEM_PADDING_X = 10` (`ListView.cpp:22`)

## Impact
- **Visual inconsistency**: Similar dialog components have different internal spacing, making the UI feel unpolished
- **Maintenance burden**: Developers cannot assume consistent spacing behavior across similar components
- **Brand coherence**: Inconsistent spacing undermines the visual identity of the UI system

## Evidence
```cpp
// MessageBox.cpp:22-23
static constexpr int BOX_PADDING = 12;
static constexpr int ICON_MARGIN = 8;

// ContextMenuView.cpp:22
static constexpr int BOX_PADDING = 8;

// InfoView.cpp:23
static constexpr int TEXT_MARGIN = 8;

// T9InputView.cpp:40
static constexpr int TEXT_MARGIN = 10;

// ListView.cpp:22
static constexpr int ITEM_PADDING_X = 10;
```

## Recommended Fix
1. Create a centralized spacing constants header file (`components/cdc_views/include/cdc_views/Spacing.h`)
2. Define a consistent spacing scale based on the display resolution (296x128):
   - `kSpaceSm = 8` (for tight spacing, icon margins)
   - `kSpaceMd = 10` (for standard padding)
   - `kSpaceLg = 12` (for prominent spacing)
3. Replace individual component constants with references to the centralized values
4. Document the spacing scale with usage guidelines

Example implementation:
```cpp
// Spacing.h
#pragma once
namespace cdc::ui {
namespace spacing {
constexpr int kSm = 8;   // Small: icon margins, tight spacing
constexpr int kMd = 10;  // Medium: standard padding
constexpr int kLg = 12;  // Large: prominent spacing
}
}
```

## References
- Material Design Spacing: https://m2.material.io/design/layout/understanding-layout.html
- E-ink display considerations: Limited resolution (296x128) requires careful spacing choices
