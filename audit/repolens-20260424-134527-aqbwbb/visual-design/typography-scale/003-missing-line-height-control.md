---
title: "[LOW] No explicit line-height control - relies on font-defined yAdvance"
severity: LOW
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The typography system has no explicit line-height control. Line spacing is determined solely by the font's `yAdvance` value in the `GFXfont` structure:

```cpp
// gfxfont.h line 17-22
typedef struct {
    uint8_t  *bitmap;      // Glyph bitmaps, concatenated
    GFXglyph *glyph;       // Glyph array
    uint8_t   first, last; // ASCII extents
    uint8_t   yAdvance;    // Newline distance (y axis)
} GFXfont;
```

Font definitions show varying yAdvance values:
- FreeMonoBold9pt7b: yAdvance = 12 (line 199)
- FreeMonoBold12pt7b: yAdvance = 17 (line 268)
- FreeMonoBold18pt7b: yAdvance = 25 (line 450)
- FreeMonoBold24pt7b: yAdvance = 34 (line ~720)

These create implicit line-height ratios:
- 9pt: 12/9 = 1.33
- 12pt: 17/12 = 1.42
- 18pt: 25/18 = 1.39
- 24pt: 34/24 = 1.42

No mechanism exists to override or adjust these values per-view or per-content-type.

## Impact
- **Readability**: Tight line spacing (1.33-1.42) may be cramped for multi-line body text
- **No adjustment**: Cannot increase line height for better readability on E-Paper
- **Inconsistent rhythm**: Different fonts have different internal spacing
- **Multi-line text**: Long text may appear cramped with no way to add leading

## Evidence
```cpp
// LockScreenView.cpp:654-670 - Multi-line info text with no line-height control
// Info line 1 (size 2 = 9pt)
gfx->setFont(FONT_SIZES[selectedSize - 1]);
gfx->setTextSize(1);
gfx->setCursor(infoX, INFO_Y);  // Y = 80
gfx->print(info_);

// Info line 2 (size 2 = 9pt)
gfx->setCursor(info2X, INFO2_Y);  // Y = 96 (16px gap, not based on line-height)
gfx->print(info2_);

// Hard-coded Y positions (lines 35-37)
static constexpr int INFO_Y = 80;       // Info line 1 (small text)
static constexpr int INFO2_Y = 96;      // Info line 2 (small text)
// 16px gap for 9pt font (12px yAdvance) = 4px extra leading
```

## Recommended Fix
**Option 1 - Add line-height helper:**
Create a helper function that calculates Y positions based on font metrics:
```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h
namespace render {
    /**
     * \brief Calculate line Y position based on previous line and font.
     * \param prevY Previous line baseline.
     * \param font Current font (nullptr for built-in).
     * \param lineHeightMultiplier Multiplier for line height (default 1.2).
     * \return Y position for next line.
     */
    int16_t nextLineY(int16_t prevY, const GFXfont* font, float lineHeightMultiplier = 1.2f);
    
    /**
     * \brief Get font's line height (yAdvance).
     */
    uint8_t getLineHeight(const GFXfont* font);
}
```

**Option 2 - Define consistent spacing constants:**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp
/**
 * \brief Line spacing constants (based on font yAdvance).
 */
static constexpr int LINE_SPACING_9PT = 14;   // 12px yAdvance + 2px gap
static constexpr int LINE_SPACING_12PT = 18;  // 17px yAdvance + 1px gap
static constexpr int LINE_SPACING_18PT = 27;  // 25px yAdvance + 2px gap

// Use for multi-line text:
int16_t y = INFO_Y;
for (const char* line : lines) {
    gfx->setCursor(0, y);
    gfx->print(line);
    y += LINE_SPACING_9PT;
}
```

**Option 3 - Document current line heights:**
```cpp
/**
 * \brief Line spacing for each font size (yAdvance values):
 * - Size 1 (built-in): 8px
 * - Size 2 (9pt): 12px
 * - Size 3 (12pt): 17px
 * - Size 4 (18pt): 25px
 * - Size 5 (24pt): 34px
 * 
 * Add 2-4px extra for comfortable leading.
 */
```

## References
- Line-height best practices: https://web.dev/line-height/
- Vertical rhythm: https://zellwk.com/blog/rhythm/
- GFXfont structure: `components/Adafruit-GFX/gfxfont.h`
