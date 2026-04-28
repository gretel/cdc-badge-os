---
title: "[MEDIUM] Hardcoded display Y-position constants across views"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Multiple view components use hardcoded Y-position values for UI layout without a centralized design system. These values (5, 15, 20, 28, 30, 50, 55, 60, 80, 85, 90, etc.) are scattered across view implementations.

**Files affected:**
- `components/cdc_views/src/DateInputView.cpp:20-23`
- `components/cdc_views/src/InfoView.cpp:21-22`
- `components/cdc_views/src/ListView.cpp:21-22`
- `components/cdc_views/src/PinEntryView.cpp:24-28`
- `components/cdc_views/src/SliderView.cpp:22-26`
- `components/cdc_views/src/T9InputView.cpp:39-40`
- `components/cdc_views/src/TimeInputView.cpp:20-23`
- `components/cdc_views/src/ContextMenuView.cpp:25`
- `components/cdc_views/src/MessageBox.cpp:25`
- `components/grove_led/src/RgbInputView.cpp:19-22`

## Impact
- **Consistency**: Different views use different spacing (e.g., TITLE_Y is 5, 15, or 20 depending on view).
- **Maintainability**: Changing the display layout requires updating multiple files.
- **Design system**: No central definition of spacing standards.

## Evidence

**Inconsistent TITLE_Y values:**
```cpp
// InfoView.cpp:21
static constexpr int TITLE_Y = 5;

// ListView.cpp:21
static constexpr int TITLE_Y = 5;

// PinEntryView.cpp:24
static constexpr int TITLE_Y = 15;

// DateInputView.cpp:20
static constexpr int TITLE_Y = 20;

// SliderView.cpp:22
static constexpr int TITLE_Y = 20;
```

**Scattered spacing values:**
```cpp
// DateInputView.cpp
static constexpr int DATE_Y = 60;
static constexpr int HINT_Y = 90;

// SliderView.cpp
static constexpr int VALUE_Y = 55;
static constexpr int BAR_Y = 85;
static constexpr int BAR_HEIGHT = 20;
static constexpr int BAR_MARGIN = 20;

// PinEntryView.cpp
static constexpr int PIN_Y = 50;
static constexpr int PIN_DOT_SPACING = 24;
static constexpr int RETRIES_Y = 80;
```

## Recommended Fix

1. **Create a centralized layout constants header** at `components/cdc_ui/include/cdc_ui/layout_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief UI layout constants for consistent display design
     * 
     * Based on 296x128 E-Paper display with standard spacing.
     */
    namespace cdc::ui {
    namespace layout {
        // Title positions
        static constexpr int TITLE_Y_COMPACT = 5;    // For simple views
        static constexpr int TITLE_Y_NORMAL = 15;    // Standard spacing
        static constexpr int TITLE_Y_LARGE = 20;     // With extra top margin
        
        // Content positions
        static constexpr int CONTENT_START_Y = 28;   // After title
        static constexpr int CONTENT_NORMAL_Y = 50;  // Standard content
        static constexpr int CONTENT_WIDE_Y = 55;    // Wider content
        
        // Element spacing
        static constexpr int SPACING_TINY = 10;
        static constexpr int SPACING_SMALL = 15;
        static constexpr int SPACING_NORMAL = 20;
        static constexpr int SPACING_LARGE = 25;
        
        // Specific element positions
        static constexpr int PIN_ENTRY_Y = 50;
        static constexpr int PIN_DOT_SPACING = 24;
        static constexpr int PIN_RETRIES_Y = 80;
        
        static constexpr int SLIDER_BAR_Y = 85;
        static constexpr int SLIDER_BAR_HEIGHT = 20;
        static constexpr int SLIDER_BAR_MARGIN = 20;
        
        static constexpr int HINT_Y = 90;
        
        // List and container positions
        static constexpr int LIST_START_Y = 30;
        static constexpr int LIST_FOOTER_HEIGHT = 16;
        
        // Max widths
        static constexpr int MAX_BOX_WIDTH = 260;
    }
    }
    ```

2. **Update views** to use centralized constants:
    ```cpp
    // Before:
    static constexpr int TITLE_Y = 20;
    static constexpr int PIN_Y = 50;
    
    // After:
    using namespace cdc::ui::layout;
    static constexpr int TITLE_Y = TITLE_Y_LARGE;
    static constexpr int PIN_Y = PIN_ENTRY_Y;
    ```

## References
- Display: E-Paper 296x128 (main badge display)
- [UI Design Patterns](https://en.wikipedia.org/wiki/User_interface_design)
