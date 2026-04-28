---
title: "[MEDIUM] Inconsistent TITLE_Y positions and icon positioning across views"
severity: MEDIUM
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary

View components use inconsistent TITLE_Y values and icon positioning strategies, creating visual drift across similar UI elements.

### TITLE_Y inconsistencies

| Component | TITLE_Y | File |
|-----------|---------|------|
| ListView | 5 | `components/cdc_views/src/ListView.cpp:21` |
| T9InputView | 5 | `components/cdc_views/src/T9InputView.cpp:39` |
| InfoView | 5 | `components/cdc_views/src/InfoView.cpp:21` |
| PinEntryView | 15 | `components/cdc_views/src/PinEntryView.cpp:24` |
| PinChangeView | 15 | `components/cdc_os_ui/src/views/PinChangeView.cpp:22` |
| SliderView | 20 | `components/cdc_views/src/SliderView.cpp:22` |
| DateInputView | 20 | `components/cdc_views/src/DateInputView.cpp:20` |
| TimeInputView | 20 | `components/cdc_views/src/TimeInputView.cpp:20` |
| RgbInputView | 20 | `components/grove_led/src/RgbInputView.cpp:19` |

### Icon positioning inconsistencies

**ToastView** uses dynamic centering:
- `iconY = boxY + (BOX_HEIGHT / 2)` at `ToastView.cpp:104`
- `textY = boxY + (BOX_HEIGHT / 2) + 4` at `ToastView.cpp:99`

**ConfirmView** uses hardcoded values:
- `iconY = boxY + 22` at `ConfirmView.cpp:92`
- `textY = boxY + 20` at `ConfirmView.cpp:87`

## Impact

1. **Visual inconsistency**: Titles appear at different heights, breaking vertical rhythm
2. **Confusing patterns**: Users must reorient when switching between views
3. **Maintenance burden**: No clear pattern for where new views should position titles
4. **Icon misalignment**: Dynamic vs. hardcoded icon positioning may cause visual drift if box heights change

## Evidence

### TITLE_Y values found across views

```cpp
// Tight spacing (5px) - list/info style views
static constexpr int TITLE_Y = 5;  // ListView, T9InputView, InfoView

// Medium spacing (15px) - PIN entry views
static constexpr int TITLE_Y = 15;  // PinEntryView, PinChangeView

// Prominent spacing (20px) - input forms
static constexpr int TITLE_Y = 20;  // SliderView, DateInputView, TimeInputView, RgbInputView
```

### Icon positioning comparison

**ToastView.cpp:98-104** (dynamic, adapts to BOX_HEIGHT)
```cpp
int textX = boxX + 15;
int textY = boxY + (BOX_HEIGHT / 2) + 4;  // Centered + offset

if (icon_ != Icon::NONE) {
    int iconX = boxX + 20;
    int iconY = boxY + (BOX_HEIGHT / 2);  // Vertically centered
```

**ConfirmView.cpp:85-92** (hardcoded, fixed positions)
```cpp
int textX = boxX + 15;
int textY = boxY + 20;  // Fixed position

if (icon_ != Icon::NONE) {
    int iconX = boxX + 20;
    int iconY = boxY + 22;  // Fixed position
```

With BOX_HEIGHT = 50 (ToastView) vs BOX_HEIGHT = 60 (ConfirmView):
- ToastView iconY: boxY + 25
- ConfirmView iconY: boxY + 22
- 3px vertical offset difference

## Recommended Fix

### Fix 1: Standardize TITLE_Y with semantic tokens

Create centralized spacing tokens in `components/cdc_views/include/cdc_views/Spacing.h`:

```cpp
namespace cdc::ui {
namespace layout {
// Title positions - semantic naming
constexpr int titleY_Tight = 5;      // For list/info views
constexpr int titleY_Medium = 15;    // For PIN entry views
constexpr int titleY_Prominent = 20; // For input forms

// When to use each:
// - titleY_Tight: Views where title is secondary to main content
// - titleY_Medium: Views needing moderate visual separation
// - titleY_Prominent: Views where title is primary focus
}
}
```

Update each view to use semantic tokens:

```cpp
// ListView.cpp
#include "cdc_views/Spacing.h"
static constexpr int TITLE_Y = layout::titleY_Tight;

// PinEntryView.cpp
#include "cdc_views/Spacing.h"
static constexpr int TITLE_Y = layout::titleY_Medium;

// DateInputView.cpp
#include "cdc_views/Spacing.h"
static constexpr int TITLE_Y = layout::titleY_Prominent;
```

### Fix 2: Unify icon positioning strategy

Option A - Use dynamic centering everywhere:

```cpp
// ConfirmView.cpp
int textY = boxY + (BOX_HEIGHT / 2) + 4;  // Match ToastView

if (icon_ != Icon::NONE) {
    int iconY = boxY + (BOX_HEIGHT / 2);  // Match ToastView
```

Option B - Use consistent hardcoded offsets:

```cpp
// ToastView.cpp
int textY = boxY + 20;  // Match ConfirmView

if (icon_ != Icon::NONE) {
    int iconY = boxY + 22;  // Match ConfirmView
```

**Recommendation**: Use Option A (dynamic centering) for dialogs where content height may vary.

### Fix 3: Document spacing rationale

Add comments explaining why specific values are chosen:

```cpp
// PinEntryView.cpp
static constexpr int TITLE_Y = 15;  // Medium spacing for PIN entry - balances title prominence with dot visibility
```

## Testing

After fixing:
1. Navigate between views and verify title alignment feels consistent
2. Test dialogs with varying text lengths
3. Verify icons are vertically centered in both ToastView and ConfirmView
4. Check that visual hierarchy is maintained across the 296×128 display

## References

- Related to issue 003 (missing centralized spacing system)
- Related to issue 006 (PIN_DOT inconsistencies)
- Display resolution: 296×128 pixels (Good Display GDEY029T94)

</content>