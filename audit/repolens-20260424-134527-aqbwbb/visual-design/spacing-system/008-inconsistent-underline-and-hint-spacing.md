---
title: "[LOW] Inconsistent UNDERLINE_Y and HINT_Y values across input views"
severity: LOW
domain: visual-design
lens: spacing-system
labels:
  - audit:visual-design/spacing-system
---

## Summary

Input views use inconsistent spacing for underlines and hints, creating visual drift across similar form elements.

### UNDERLINE_Y inconsistencies

| Component | UNDERLINE_Y calculation | Result (approx) | File |
|-----------|------------------------|-----------------|------|
| DateInputView | `DATE_Y + 20` | 80 | `components/cdc_views/src/DateInputView.cpp:22` |
| RgbInputView | `RGB_Y + 20` | 75 | `components/grove_led/src/RgbInputView.cpp:21` |
| TimeInputView | `TIME_Y + 25` | 80 | `components/cdc_views/src/TimeInputView.cpp:22` |

### HINT_Y inconsistencies

| Component | HINT_Y | File |
|-----------|--------|------|
| DateInputView | 90 | `components/cdc_views/src/DateInputView.cpp:23` |
| TimeInputView | 90 | `components/cdc_views/src/TimeInputView.cpp:23` |
| RgbInputView | 115 | `components/grove_led/src/RgbInputView.cpp:23` |

### Key observations

1. **UNDERLINE_Y**: TimeInputView uses +25 offset while DateInputView and RgbInputView use +20
2. **HINT_Y**: RgbInputView places hint at 115 (25px lower) compared to DateInputView and TimeInputView at 90

## Impact

1. **Visual inconsistency**: Similar input fields have different spacing patterns
2. **Confusing patterns**: No clear rule for where underlines and hints should be placed
3. **Maintenance burden**: New input views must guess appropriate values

## Evidence

### DateInputView.cpp:20-23
```cpp
static constexpr int TITLE_Y = 20;
static constexpr int DATE_Y = 60;
static constexpr int UNDERLINE_Y = DATE_Y + 20;  // = 80
static constexpr int HINT_Y = 90;                 // 10px below underline
```

### TimeInputView.cpp:20-23
```cpp
static constexpr int TITLE_Y = 20;
static constexpr int TIME_Y = 55;
static constexpr int UNDERLINE_Y = TIME_Y + 25;  // = 80
static constexpr int HINT_Y = 90;                 // 10px below underline
```

### RgbInputView.cpp:19-23
```cpp
static constexpr int TITLE_Y = 20;
static constexpr int RGB_Y = 55;
static constexpr int UNDERLINE_Y = RGB_Y + 20;  // = 75
static constexpr int PREVIEW_Y = 95;             // Additional element
static constexpr int HINT_Y = 115;               // 20px below PREVIEW
```

### Layout comparison

**DateInputView** (296×128 display):
```
TITLE_Y = 20
DATE_Y = 60
UNDERLINE_Y = 80 (20px below date)
HINT_Y = 90 (10px below underline)
```

**TimeInputView** (296×128 display):
```
TITLE_Y = 20
TIME_Y = 55
UNDERLINE_Y = 80 (25px below time)
HINT_Y = 90 (10px below underline)
```

**RgbInputView** (296×128 display):
```
TITLE_Y = 20
RGB_Y = 55
UNDERLINE_Y = 75 (20px below RGB)
PREVIEW_Y = 95 (20px below underline)
HINT_Y = 115 (20px below preview)
```

The inconsistency:
- DateInputView: 20px gap to underline
- TimeInputView: 25px gap to underline (5px more)
- RgbInputView: 20px gap to underline, but 20px gap to hint (vs 10px in others)

## Recommended Fix

### Standardize spacing gaps

Create consistent spacing tokens in `components/cdc_views/include/cdc_views/Spacing.h`:

```cpp
namespace cdc::ui {
namespace layout {
// Input field spacing
constexpr int inputFieldToUnderline = 20;
constexpr int underlineToHint = 10;
constexpr int inputFieldToPreview = 20;
constexpr int previewToHint = 20;
}
}
```

### Update each view

**DateInputView.cpp**
```cpp
#include "cdc_views/Spacing.h"
static constexpr int TITLE_Y = 20;
static constexpr int DATE_Y = 60;
static constexpr int UNDERLINE_Y = DATE_Y + layout::inputFieldToUnderline;
static constexpr int HINT_Y = UNDERLINE_Y + layout::underlineToHint;
```

**TimeInputView.cpp**
```cpp
#include "cdc_views/Spacing.h"
static constexpr int TITLE_Y = 20;
static constexpr int TIME_Y = 55;
static constexpr int UNDERLINE_Y = TIME_Y + layout::inputFieldToUnderline;  // Was +25, now +20
static constexpr int HINT_Y = UNDERLINE_Y + layout::underlineToHint;
```

**RgbInputView.cpp**
```cpp
#include "cdc_views/Spacing.h"
static constexpr int TITLE_Y = 20;
static constexpr int RGB_Y = 55;
static constexpr int UNDERLINE_Y = RGB_Y + layout::inputFieldToUnderline;
static constexpr int PREVIEW_Y = UNDERLINE_Y + layout::inputFieldToPreview;
static constexpr int HINT_Y = PREVIEW_Y + layout::previewToHint;
```

### Consider RgbInputView layout

RgbInputView has an extra PREVIEW element, so the spacing is different by design. However, the gap values should still be consistent:
- UNDERLINE to PREVIEW: 20px (same as input to underline)
- PREVIEW to HINT: 20px (could be reduced to 15px for tighter rhythm)

## Testing

After fixing:
1. Test all input views (Date, Time, RGB)
2. Verify underline positions are consistent relative to input fields
3. Verify hint positions follow a predictable pattern
4. Check that RgbInputView's extra PREVIEW element doesn't cause visual crowding

## References

- Related to issue 003 (missing centralized spacing system)
- Related to issue 007 (TITLE_Y inconsistencies)
- Display resolution: 296×128 pixels (Good Display GDEY029T94)

</content>