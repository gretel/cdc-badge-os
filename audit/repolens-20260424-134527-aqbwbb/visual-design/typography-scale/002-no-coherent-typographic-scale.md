---
title: "[MEDIUM] No coherent typographic scale - arbitrary textSize values used without design tokens"
severity: MEDIUM
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The typography system uses `setTextSize(N)` where N is an integer (1-5), but these values are chosen ad-hoc in different views without a consistent design system. The mapping is:

| Size | Font | Point Size | Approx Pixels |
|------|------|------------|---------------|
| 1 | Built-in bitmap | ~6pt | 6x8 |
| 2 | FreeMonoBold | 9pt | ~11px |
| 3 | FreeMonoBold | 12pt | ~16px |
| 4 | FreeMonoBold | 18pt | ~24px |
| 5 | FreeMonoBold | 24pt | ~32px |

The progression (9, 12, 18, 24) does not follow a standard typographic scale ratio (e.g., 1.25, 1.333, 1.5, or golden ratio 1.618).

**Inconsistent usage patterns found:**
- `LockScreenView.cpp:598-600`: Clock uses `setTextSize(2)` with built-in font
- `LockScreenView.cpp:602`: Date uses `setTextSize(1)` with built-in font
- `LockScreenView.cpp:628`: Name uses textSize 1 with variable font selection
- `TotpModule.cpp:424`: TOTP code uses `setTextSize(2)` 
- `TimeInputView.cpp:193`: Time input uses `setTextSize(3)`

## Impact
- **Visual inconsistency**: Similar UI elements may use different sizes for no clear reason
- **Maintenance burden**: Adding new views requires guessing appropriate sizes
- **Poor hierarchy**: The scale ratios (9→12 = 1.33, 12→18 = 1.5, 18→24 = 1.33) are inconsistent
- **No responsive baseline**: Cannot easily adapt scale for different display resolutions

## Evidence
```cpp
// Inconsistent sizing across views:

// LockScreenView.cpp:598-600 - Clock (header)
gfx->setFont(nullptr);
gfx->setTextSize(2);  // Built-in font, size 2 multiplier

// LockScreenView.cpp:602 - Date (sub-header)
gfx->setTextSize(1);  // Built-in font, size 1 multiplier

// LockScreenView.cpp:628 - Name (main content)
gfx->setFont(FONT_SIZES[selectedSize - 1]);
gfx->setTextSize(1);  // Variable font, size 1 multiplier

// TotpModule.cpp:424 - TOTP code display
gfx->setTextSize(2);  // Built-in font, size 2 multiplier

// TimeInputView.cpp:193 - Time input
gfx->setTextSize(3);  // Built-in font, size 3 multiplier
```

The actual point sizes available (9, 12, 18, 24) don't follow a standard scale:
- Major third scale (1.25): 9 → 11.25 → 14.06 → 17.58
- Perfect fourth scale (1.333): 9 → 12 → 16 → 21.3
- Augmented fourth scale (1.414): 9 → 12.7 → 18 → 25.5
- Golden ratio (1.618): 9 → 14.6 → 23.6

Current scale (9, 12, 18, 24) mixes 1.33 and 1.5 ratios inconsistently.

## Recommended Fix
Define a coherent typography scale in a central location:

**Option 1 - Define standard scale ratios:**
```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h
namespace typography {
    // Standard modular scale (perfect fourths: 1.333 ratio)
    constexpr float SCALE_RATIO = 1.333f;
    
    // Pre-calculated sizes for 9pt base
    enum class TextSize {
        XS = 1,  // ~6pt (built-in)
        SM = 2,  // ~9pt
        BASE = 3, // ~12pt
        LG = 4,   // ~16pt (would need 16pt font)
        XL = 5    // ~24pt
    };
    
    // Helper to get appropriate font for size
    const GFXfont* getFontForSize(TextSize size);
}
```

**Option 2 - Use semantic naming:**
```cpp
// Replace magic numbers with semantic constants
enum class TextRole {
    CAPTION,      // Built-in 6x8
    BODY,         // 9pt
    SUBTITLE,     // 12pt  
    HEADING,      // 18pt
    TITLE         // 24pt
};

// Usage:
gfx->setFont(getFontForRole(TextRole::HEADING));
gfx->setTextSize(1);  // Always 1 when using pre-scaled fonts
```

**Option 3 - Document the current scale:**
If keeping current sizes, document them clearly:
```cpp
/**
 * \brief Typography scale for 296x128 E-Paper display.
 * 
 * Size mapping (FreeMonoBold):
 * - 1: Built-in 6x8 bitmap (6pt equivalent)
 * - 2: 9pt (11px) - captions, small labels
 * - 3: 12pt (16px) - body text, list items
 * - 4: 18pt (24px) - section headings
 * - 5: 24pt (32px) - main titles, large numbers
 */
```

## References
- Modular scale: https://www.modularscale.com/
- Typography for small displays: https://web.dev/font-size/
- Current font definitions: `components/Adafruit-GFX/Fonts/FreeMonoBold*.h`
