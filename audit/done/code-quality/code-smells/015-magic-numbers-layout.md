---
title: "[LOW] Magic Numbers: Display constants scattered across view files"
severity: LOW
domain: ui
lens: code-smells
labels:
  - "magic-numbers"
  - "cdc_views"
---

## Summary
Display layout constants like `TITLE_Y = 20`, `TIME_Y = 55`, `TEXT_MARGIN = 10` are scattered across view files without a centralized configuration. These magic numbers make it hard to adjust layout globally.

## Impact
**Inconsistency**: Different views may use slightly different values for the same purpose.

**Hard to theme**: Changing layout requires editing multiple files.

**Discoverability**: Hard to find all places that use a specific layout value.

## Evidence
`components/cdc_views/src/TimeInputView.cpp:18-22`:
```cpp
static constexpr int TITLE_Y = 20;
static constexpr int TIME_Y = 55;
static constexpr int UNDERLINE_Y = TIME_Y + 25;
static constexpr int HINT_Y = 90;
```

`components/cdc_views/src/DateInputView.cpp:18-22`:
```cpp
static constexpr int TITLE_Y = 20;
static constexpr int DATE_Y = 60;
static constexpr int UNDERLINE_Y = DATE_Y + 20;
static constexpr int HINT_Y = 90;
```

`components/cdc_views/src/T9InputView.cpp:38-41`:
```cpp
static constexpr int TITLE_Y = 5;
static constexpr int TEXT_Y = 50;
static constexpr int TEXT_MARGIN = 10;
```

Same values (TITLE_Y=20, HINT_Y=90) appear in multiple places but with slight variations.

## Recommended Fix
1. **Create layout constants header**:
```cpp
// components/cdc_views/include/cdc_views/LayoutConstants.h
#pragma once

namespace cdc::ui::layout {
    constexpr int HEADER_Y = 20;
    constexpr int FOOTER_Y = 90;
    constexpr int TEXT_MARGIN = 10;
    constexpr int INPUT_HEIGHT = 30;
    constexpr int ITEM_HEIGHT = 20;
    constexpr int TITLE_FONT_SIZE = 1;
    constexpr int BODY_FONT_SIZE = 2;
}
```

2. **Update views to use constants**:
```cpp
#include "cdc_views/LayoutConstants.h"

void TimeInputView::render(bool partial) {
    gfx->setTextSize(layout::TITLE_FONT_SIZE);
    render::drawHeaderCentered(gfx, title_, layout::HEADER_Y, width);
    // ...
}
```

**Estimated effort**: ~1 hour to create constants and update 3-4 view files.

## References
- Refactoring.com: "Magic Numbers" - https://refactoring.com/catalog/extractMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
