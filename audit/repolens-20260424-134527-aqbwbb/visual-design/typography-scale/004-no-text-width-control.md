---
title: "[LOW] No text container width control - lines can exceed readable measure"
severity: LOW
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The typography system has no mechanism to constrain text width for optimal readability. Text can span the full 296px display width, potentially creating lines too long for comfortable reading.

For E-Paper displays with the available fonts:
- **Built-in 6x8 font**: ~6px per character → 49 characters at full width
- **9pt FreeMonoBold**: ~9px per character → 33 characters at full width
- **12pt FreeMonoBold**: ~12px per character → 25 characters at full width

Recommended measure for readability is 45-75 characters per line. The current system has no way to enforce this.

## Impact
- **Readability**: Long lines make it harder for eyes to track from end of one line to start of next
- **Consistency**: No guarantee of consistent text width across views
- **Responsive design**: Cannot adapt text width for different display sizes

## Evidence
```cpp
// LockScreenView.cpp:654-670 - Info text rendered without width constraint
gfx->setCursor(infoX, INFO_Y);
gfx->print(info_);  // No max-width check, can span full 296px

// TotpModule.cpp:398-410 - Code display without measure control
gfx->setCursor(8, 28);
gfx->print(name_);  // Full width unless manually calculated

// All views use setCursor(x, y) + print() with no container width
```

Display width is 296px (from `LockScreenView.cpp:38`):
```cpp
static constexpr int DISPLAY_WIDTH = 296;
```

No `max-width` or text wrapping logic exists in the IDisplay interface or views.

## Recommended Fix
**Option 1 - Add text wrapping to IDisplay:**
```cpp
// components/cdc_hal/include/cdc_hal/IDisplay.h
/**
 * \brief Print text with word wrapping.
 * \param text Text to print.
 * \param maxWidth Maximum width in pixels.
 * \param x Left position (cursor).
 * \param y Top position (cursor).
 */
virtual void printWrapped(const char* text, uint16_t maxWidth, int16_t x, int16_t y) = 0;
```

**Option 2 - Add text width constraint helper:**
```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h
namespace render {
    /**
     * \brief Get optimal max-width for readable text.
     * \param font Current font.
     * \param charsPerLine Target characters per line (45-75).
     * \return Pixel width for text container.
     */
    uint16_t optimalTextWidth(const GFXfont* font, int charsPerLine = 50);
    
    /**
     * \brief Calculate wrapped lines count.
     * \param text Text to measure.
     * \param maxWidth Maximum width in pixels.
     * \return Number of lines needed.
     */
    uint8_t countWrappedLines(const char* text, uint16_t maxWidth);
}
```

**Option 3 - Document text width guidelines:**
```cpp
/**
 * \brief Text width guidelines for 296px display:
 * 
 * Recommended max-widths for readable measure (45-75 chars):
 * - Built-in (6px): 270px (45 chars)
 * - 9pt (9px): 270px (30 chars)
 * - 12pt (12px): 240px (20 chars)
 * - 18pt (18px): 216px (12 chars)
 * - 24pt (24px): 192px (8 chars)
 * 
 * Use centered text with these widths for best readability.
 */
```

## References
- Optimal line length: https://baymard.com/blog/optimal-line-length-readability
- Measure (typography): https://en.wikipedia.org/wiki/Measure_(typography)
- Display specs: `components/cdc_hal/include/cdc_hal/IDisplay.h`
