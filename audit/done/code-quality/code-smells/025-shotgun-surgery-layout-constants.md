---
title: "[MEDIUM] Shotgun Surgery: Display dimension constants scattered across views"
severity: MEDIUM
domain: cdc_views
lens: code-smells
labels:
  - "refactor:consolidate-constants"
  - "maintainability"
---

## Summary
Display dimension constants (TITLE_Y, LIST_START_Y, ITEM_PADDING_X, etc.) are duplicated across multiple view files. Changing the display layout requires modifying many files.

**Location:** Multiple view files in `cdc_views`

## Evidence
```cpp
// ListView.cpp:18-26
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;

// DateInputView.cpp:17-20
static constexpr int TITLE_Y = 20;
static constexpr int DATE_Y = 60;
static constexpr int UNDERLINE_Y = DATE_Y + 20;
static constexpr int HINT_Y = 90;

// TimeInputView.cpp:17-20
static constexpr int TITLE_Y = 20;
static constexpr int TIME_Y = 55;
static constexpr int UNDERLINE_Y = TIME_Y + 20;
static constexpr int HINT_Y = 90;

// DateInputView.cpp:234-236
gfx->setTextSize(2);
int16_t x1, y1;
uint16_t w, h;
gfx->getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
int startX = (width - w) / 2;
```

These constants are duplicated in:
- `ListView.cpp`
- `DateInputView.cpp`  
- `TimeInputView.cpp`
- `ConfirmView.cpp`
- `MessageBox.cpp`
- And likely more view files

## Impact
- **High churn**: Layout changes require editing 5+ files
- **Inconsistency risk**: Not all constants updated uniformly
- **Maintenance burden**: Hard to understand global layout structure
- **Error-prone**: Easy to miss one constant during updates

## Recommended Fix
Centralize display layout constants in a single configuration file:

```cpp
// cdc_views/include/cdc_views/DisplayLayout.h
#pragma once

namespace cdc::ui::layout {

// Display dimensions (derived from hardware)
constexpr uint16_t DISPLAY_WIDTH = 296;
constexpr uint16_t DISPLAY_HEIGHT = 128;

// Common spacing
constexpr int MARGIN_X = 10;
constexpr int MARGIN_Y = 5;
constexpr int HEADER_HEIGHT = 20;
constexpr int FOOTER_HEIGHT = 16;
constexpr int ITEM_HEIGHT = 18;

// List view
constexpr int LIST_START_Y = 30;
constexpr int SCROLL_INDICATOR_WIDTH = 8;

// Input views
constexpr int DATE_Y = 60;
constexpr int TIME_Y = 55;
constexpr int UNDERLINE_HEIGHT = 4;
constexpr int HINT_Y = 90;

// Dialog views
constexpr int DIALOG_PADDING = 15;
constexpr int DIALOG_MIN_WIDTH = 200;

} // namespace cdc::ui::layout
```

Usage:
```cpp
// DateInputView.cpp
#include "cdc_views/DisplayLayout.h"

void DateInputView::render(bool partial) {
    using namespace cdc::ui::layout;
    
    // Use constants
    gfx->setCursor((width - w) / 2, DATE_Y);
    gfx->fillRect(0, DATE_Y + 20, width, UNDERLINE_HEIGHT, EPD_WHITE);
    // ...
}
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Shotgun Surgery smell
- DRY principle: Every piece of knowledge must have a single, unambiguous representation
