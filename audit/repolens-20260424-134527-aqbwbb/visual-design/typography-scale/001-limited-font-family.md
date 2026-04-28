---
title: "[MEDIUM] Limited font family utilization - only FreeMonoBold used despite available alternatives"
severity: MEDIUM
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The CDC Badge OS typography system has access to a comprehensive set of Adafruit GFX fonts in `components/Adafruit-GFX/Fonts/` including:
- FreeMono (monospace): 9pt, 12pt, 18pt, 24pt (regular, bold, oblique, bold oblique)
- FreeSans (sans-serif): 9pt, 12pt, 18pt, 24pt (regular, bold, oblique, bold oblique)
- FreeSerif (serif): 9pt, 12pt, 18pt, 24pt (regular, bold, italic, bold italic)
- Special fonts: Picopixel, Tiny3x3a2pt7b, TomThumb, Org_01

However, the codebase **exclusively uses FreeMonoBold** variants:
- `components/cdc_os_ui/src/views/LockScreenView.cpp:18-21` - imports only FreeMonoBold fonts
- `components/cdc_views/src/QRCodeView.cpp:15` - imports only FreeMonoBold9pt7b

No other font families (FreeSans, FreeSerif) are utilized anywhere in the codebase despite being available.

## Impact
- **Visual variety**: Limited visual hierarchy options. Sans-serif (FreeSans) would be better for body text readability on E-Paper displays.
- **Design flexibility**: Cannot differentiate UI elements using font families (e.g., headings vs. body, emphasis styles).
- **Accessibility**: Serif fonts could provide better character distinction for certain use cases.
- **Wasted resources**: Font files occupy flash space but provide no value if unused.

## Evidence
```cpp
// LockScreenView.cpp:18-21
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

// Font size table (lines 46-49)
static const GFXfont* const FONT_SIZES[] = {
    nullptr,                // Size 1: built-in 6x8
    &FreeMonoBold9pt7b,     // Size 2: 9pt
    &FreeMonoBold12pt7b,    // Size 3: 12pt
    &FreeMonoBold18pt7b,    // Size 4: 18pt
    &FreeMonoBold24pt7b,    // Size 5: 24pt
};
```

Available but unused fonts in `components/Adafruit-GFX/Fonts/`:
- FreeSans9pt7b.h through FreeSans24pt7b.h (and variants)
- FreeSerif9pt7b.h through FreeSerif24pt7b.h (and variants)

## Recommended Fix
Evaluate whether adding font family variety improves the UI:

**Option 1 - Minimal (Recommended for E-Paper):**
Keep FreeMonoBold for headings/numbers (monospace helps alignment), add FreeSans for body text:
```cpp
// Define two font families
static const GFXfont* HEADINGS[] = {
    nullptr,                // Size 1: built-in
    &FreeMonoBold9pt7b,     // Size 2
    &FreeMonoBold12pt7b,    // Size 3
    &FreeMonoBold18pt7b,    // Size 4
    &FreeMonoBold24pt7b,    // Size 5
};

static const GFXfont* BODY[] = {
    nullptr,                // Size 1: built-in
    &FreeSans9pt7b,         // Size 2
    &FreeSans12pt7b,        // Size 3
    &FreeSans18pt7b,        // Size 4
    &FreeSans24pt7b,        // Size 5
};
```

**Option 2 - Full exploration:**
Create a typography token system in `components/cdc_views/include/cdc_views/RenderHelpers.h`:
```cpp
enum class FontFamily {
    MONOSPACE,  // FreeMonoBold - for numbers, codes, data
    SANS,       // FreeSans - for body text, labels
    SERIF       // FreeSerif - for emphasis, titles (optional)
};
```

Then update views to select appropriate family based on content type.

## References
- Adafruit GFX Fonts directory: `components/Adafruit-GFX/Fonts/`
- Current font usage: `components/cdc_os_ui/src/views/LockScreenView.cpp:18-21`
- E-Paper display characteristics: 296x128 pixels, high contrast, good for monospace alignment
