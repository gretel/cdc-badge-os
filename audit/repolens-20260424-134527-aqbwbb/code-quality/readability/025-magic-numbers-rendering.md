---
title: "[025] [MEDIUM] Magic numbers in display rendering calculations"
severity: MEDIUM
domain: code-quality/readability
lens: unclear-magic-numbers
labels:
  - "audit:code-quality/readability"
---

## Summary
Multiple display rendering functions use unexplained magic numbers for layout calculations, making it difficult to understand the visual layout and modify spacing/positioning without reverse-engineering the math.

**Affected files:**
- `components/cdc_os_ui/src/views/LockScreenView.cpp:24-38` - Lock screen layout constants
- `components/cdc_views/src/ListView.cpp:17-24` - List view layout constants
- `components/cdc_views/src/T9InputView.cpp:36-39` - T9 input layout constants

## Impact
**Maintainability:** Developers cannot quickly understand the visual layout or make consistent spacing adjustments.

**Bug risk:** Changing one spacing value may require manual recalculation of dependent values, increasing the chance of visual glitches.

**Onboarding:** New developers need to trace through rendering code or run the firmware to understand what each number represents.

## Evidence

### LockScreenView.cpp - Unclear positional magic numbers
```cpp
static constexpr int CLOCK_Y = 5;
static constexpr int DATE_Y = 22;
static constexpr int ICONS_Y = 5;
static constexpr int NAME_Y = 60;       // Name position (FreeFont baseline)
static constexpr int INFO_Y = 80;       // Info line 1 (small text)
static constexpr int INFO2_Y = 96;      // Info line 2 (small text)
static constexpr int BATTERY_X = 260;
static constexpr int BATTERY_Y = 5;
static constexpr int DISPLAY_WIDTH = 296;
```

**Problems:**
- `CLOCK_Y = 5`, `ICONS_Y = 5`, `BATTERY_Y = 5`: Why 5? Is this font baseline or top alignment?
- `DATE_Y = 22`: What determines this value? 5 + 17 (line height)?
- `NAME_Y = 60`: How was this calculated? What's the vertical hierarchy?
- `INFO_Y = 80`, `INFO2_Y = 96`: 16px spacing between info lines?
- `BATTERY_X = 260`: Why 260? Display width (296) - 36px for battery icon?

### ListView.cpp - Unexplained spacing constants
```cpp
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;
```

**Problems:**
- `LIST_START_Y = 30`: Header takes 5px + underline + spacing? What's the exact calculation?
- `TITLE_Y = 5`: Same question as above - is this consistent with other views?

### T9InputView.cpp - Layout math without explanation
```cpp
static constexpr int TITLE_Y = 5;
static constexpr int TEXT_Y = 50;
static constexpr int TEXT_MARGIN = 10;
```

**Problems:**
- `TEXT_Y = 50`: Why 50? Title (5) + header bar (20) + spacing (25)?
- No comment explaining the vertical hierarchy

## Recommended Fix

### 1. Add explanatory comments to all layout constants

**Before:**
```cpp
static constexpr int CLOCK_Y = 5;
static constexpr int DATE_Y = 22;
```

**After:**
```cpp
// === Lock Screen Layout Constants ===
// Display: 296x128 (WxH), e-paper with 6x8 built-in font (size 1)
// Header area: 25px (clock + date + icons + battery)
// Body area: 70px (name + info lines)
// Footer area: 16px (hint text)

// Header: Clock (top-left, baseline at 5px for size-2 font)
static constexpr int CLOCK_Y = 5;
// Header: Date below clock (5px + 17px line spacing)
static constexpr int DATE_Y = 22;
// Header: Icons and battery aligned with clock baseline
static constexpr int ICONS_Y = 5;
static constexpr int BATTERY_Y = 5;
// Battery icon right-aligned: 296 - 28 (icon width) - 8 (margin) = 260
static constexpr int BATTERY_X = 260;

// Body: Name centered (60px from top, 12pt font baseline)
static constexpr int NAME_Y = 60;
// Body: Info line 1 below name (80px, 9pt font baseline)
static constexpr int INFO_Y = 80;
// Body: Info line 2 below info1 (96px, 16px spacing)
static constexpr int INFO2_Y = 96;
```

### 2. Create a layout documentation section at the top of each view file

Add a diagram showing the visual layout:
```cpp
/**
 * Lock Screen Layout (296x128):
 * 
 * [5px]  CLOCK (2x size)     [260px] BATTERY + ICONS
 * [22px] DATE (1x size)
 * 
 * [60px]  NAME (12pt, centered)
 * [80px]  INFO1 (9pt, centered)
 * [96px]  INFO2 (9pt, centered)
 * 
 * [112px] FOOTER HINT (6x8 font)
 */
```

### 3. Use derived constants for dependent values

**Before:**
```cpp
static constexpr int DATE_Y = 22;
static constexpr int INFO_Y = 80;
static constexpr int INFO2_Y = 96;
```

**After:**
```cpp
static constexpr int CLOCK_Y = 5;
static constexpr int LINE_HEIGHT_SMALL = 17;  // 6x8 font + spacing
static constexpr int DATE_Y = CLOCK_Y + LINE_HEIGHT_SMALL;  // 22
static constexpr int INFO_SPACING = 16;
static constexpr int INFO_Y = NAME_Y + 20;  // After name
static constexpr int INFO2_Y = INFO_Y + INFO_SPACING;  // 96
```

### 4. Consider using a layout configuration struct

For views with many interrelated constants:
```cpp
struct LockScreenLayout {
    // Header
    static constexpr int HEADER_HEIGHT = 25;
    static constexpr int CLOCK_Y = 5;
    static constexpr int DATE_Y = CLOCK_Y + 17;
    
    // Body
    static constexpr int NAME_Y = 60;
    static constexpr int INFO_Y = NAME_Y + 20;
    static constexpr int INFO2_Y = INFO_Y + 16;
    
    // Footer
    static constexpr int FOOTER_Y = 112;
    static constexpr int FOOTER_HEIGHT = 16;
    
    // Alignment
    static constexpr int MARGIN_X = 5;
    static constexpr int BATTERY_X = DISPLAY_WIDTH - 36;  // 260
};
```

## References
- [Google C++ Style Guide - Constants](https://google.github.io/styleguide/cppguide.html#Constant_Names)
- [Clean Code - Chapter 2: Meaningful Names](https://cleancodestudent.com/clean-code-chapter-2/)
- ESP32 E-Paper display common layout patterns (296x128 resolution)
