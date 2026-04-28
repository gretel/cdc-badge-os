---
title: "[MEDIUM] No shared layout constants for dashboard components"
severity: MEDIUM
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

Dashboard/overview components use **hardcoded layout constants** defined locally in each component instead of sharing a common layout system. This affects:

- `components/cdc_os_ui/src/views/LockScreenView.cpp:29-37` - Lock screen layout constants
- `components/cdc_views/src/ListView.cpp:21-24` - ListView layout constants
- `components/cdc_views/src/RenderHelpers.cpp` - (potential shared rendering helpers)

**Current situation:**
- Lock screen defines `CLOCK_Y = 5`, `ICONS_Y = 5`, `BATTERY_Y = 5`
- ListView defines `TITLE_Y = 5`
- No shared header/footer height constants
- No consistent spacing values across components

## Impact

**Maintenance Impact:**
- Changing the header height requires updating multiple files
- Inconsistent spacing may lead to visual misalignment
- Harder to create new dashboard widgets that match existing style

**Design Impact:**
- No single source of truth for layout dimensions
- Risk of drift over time as components evolve independently
- New developers must guess appropriate spacing values

## Evidence

**Lock Screen Layout Constants:**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:29-37
static constexpr int CLOCK_Y = 5;
static constexpr int DATE_Y = 22;
static constexpr int ICONS_Y = 5;
static constexpr int NAME_Y = 60;
static constexpr int INFO_Y = 80;
static constexpr int INFO2_Y = 96;
static constexpr int BATTERY_X = 260;
static constexpr int BATTERY_Y = 5;
static constexpr int DISPLAY_WIDTH = 296;
```

**ListView Layout Constants:**
```cpp
// components/cdc_views/src/ListView.cpp:21-24
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;
```

**Observations:**
- Both use `Y = 5` for top elements (clock/title)
- No shared header height constant (lock screen header = ~22px, ListView = ~30px)
- No shared footer height constant
- No shared padding constants

## Recommended Fix

Create a shared layout constants header:

1. **Create new file** `components/cdc_hal/include/cdc_hal/DisplayLayout.h`:
   ```cpp
   namespace cdc::hal {
   
   // Header/Footer heights
   static constexpr int HEADER_HEIGHT = 30;
   static constexpr int FOOTER_HEIGHT = 16;
   static constexpr int TITLE_Y = 5;
   
   // Spacing
   static constexpr int ITEM_PADDING_X = 10;
   static constexpr int ITEM_PADDING_Y = 2;
   static constexpr int SECTION_GAP = 10;
   
   // Display dimensions (should match IDisplay)
   static constexpr int DISPLAY_WIDTH = 296;
   static constexpr int DISPLAY_HEIGHT = 128;
   }
   ```

2. **Update LockScreenView** to use shared constants:
   ```cpp
   #include "cdc_hal/DisplayLayout.h"
   // Replace local constants with hal::HEADER_HEIGHT, etc.
   ```

3. **Update ListView** to use shared constants:
   ```cpp
   #include "cdc_hal/DisplayLayout.h"
   // Replace local constants with hal::HEADER_HEIGHT, etc.
   ```

This is ~1 hour of work and improves code maintainability.

## References

- Dashboard pattern: Shared layout constants ensure visual consistency
- DRY principle: Extract common values to single source of truth
- Related: Footer hints use `drawFooterBar()` helper - check if layout constants are already used there

</content>