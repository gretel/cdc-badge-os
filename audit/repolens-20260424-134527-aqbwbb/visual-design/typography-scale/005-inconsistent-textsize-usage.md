---
title: "[LOW] Inconsistent textSize usage across views - no semantic sizing convention"
severity: LOW
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
Across the view components, `setTextSize()` values are chosen ad-hoc without a semantic naming convention or consistent pattern. The same visual hierarchy (headers, body, captions) uses different sizes in different views.

**Observed patterns:**
- **Most views**: Use `setTextSize(1)` for body/caption text
- **TimeInputView**: Uses `setTextSize(3)` for main time display
- **DateInputView**: Uses `setTextSize(2)` for date display
- **SliderView**: Uses `setTextSize(2)` for current value, `setTextSize(1)` for labels
- **LockScreenView**: Mixed usage - `setTextSize(2)` for clock, `setTextSize(1)` for date

No clear convention exists for when to use which size.

## Impact
- **Visual inconsistency**: Similar UI elements (time, date, values) rendered at different sizes
- **Learning curve**: Developers must inspect each view to understand sizing choices
- **Maintenance**: Adding similar views requires reverse-engineering sizing decisions
- **Brand consistency**: No guarantee of consistent visual language across the app

## Evidence
```cpp
// TimeInputView.cpp:193 - Time display
gfx->setTextSize(3);  // 12pt FreeMonoBold ~16px

// DateInputView.cpp:234 - Date display  
gfx->setTextSize(2);  // 9pt FreeMonoBold ~11px

// SliderView.cpp:173 - Current value
gfx->setTextSize(2);  // 9pt FreeMonoBold ~11px

// LockScreenView.cpp:599 - Clock display
gfx->setTextSize(2);  // Built-in font, 2x multiplier ~12px

// All other views: Body text
gfx->setTextSize(1);  // Built-in font, 1x multiplier ~6px
```

**Same content type, different sizes:**
- Time display: `setTextSize(3)` (TimeInputView)
- Date display: `setTextSize(2)` (DateInputView)
- Clock display: `setTextSize(2)` (LockScreenView, but with built-in font)

## Recommended Fix
**Option 1 - Define semantic size constants:**
```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h
namespace typography {
    // Semantic size names for consistency
    enum class Size {
        CAPTION = 1,    // Small labels, hints
        BODY = 1,       // Regular text
        SUBTITLE = 2,   // Section headers, secondary info
        HEADING = 3,    // Main titles, primary values
        TITLE = 4       // Large headers
    };
}

// Usage:
gfx->setTextSize(static_cast<uint8_t>(typography::Size::HEADING));
```

**Option 2 - Create helper functions:**
```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h
namespace render {
    void setBodyText(Gdey029T94* gfx);
    void setHeadingText(Gdey029T94* gfx);
    void setTitleText(Gdey029T94* gfx);
    void setCaptionText(Gdey029T94* gfx);
}

// Usage:
render::setHeadingText(gfx);
gfx->print(time_);
```

**Option 3 - Document current conventions:**
```cpp
/**
 * \brief Current textSize conventions (document for consistency):
 * 
 * - Size 1 (built-in 6x8): Body text, captions, hints, labels
 * - Size 2 (built-in 12x16): Secondary info, date, slider values
 * - Size 3 (built-in 18x24): Primary values, time display
 * - Size 1 with 9pt font: Alternative body text
 * - Size 2 with 12pt font: Alternative headings
 */
```

## References
- Design tokens: https://design-tokens.com/
- Consistent typography: https://material.io/design/typography/the-type-system.html
