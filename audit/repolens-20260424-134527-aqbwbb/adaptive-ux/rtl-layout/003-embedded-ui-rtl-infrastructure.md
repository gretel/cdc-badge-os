---
title: "[LOW] Embedded UI lacks RTL text alignment infrastructure for future languages"
severity: LOW
domain: cdc_views
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary
The embedded C++ UI system (`components/cdc_views/`) uses hardcoded left-to-right text rendering with no infrastructure for RTL languages. Key files:

- `components/cdc_views/src/RenderHelpers.cpp:23-31` - `drawHeaderLeft()` always renders left-aligned
- `components/cdc_views/src/ListView.cpp:237-246` - Text cursor always starts at `textX = ITEM_PADDING_X`
- `components/cdc_views/src/InfoView.cpp:195-205` - Text rendered left-to-right with fixed cursor position
- `components/cdc_views/src/T9InputView.cpp:295-321` - Input text always left-aligned

Current I18n support (`components/cdc_ui/include/cdc_ui/I18n.h`) only provides English and German translations, but no mechanism to switch text direction based on language.

## Impact
While currently limited to LTR languages (English, German), adding RTL languages (Arabic, Hebrew) would require:
- Refactoring all view rendering code to support direction-aware text alignment
- Adding RTL support to footer hints, headers, and list items
- Modifying `I18n` to track text direction per language

## Evidence
```cpp
// RenderHelpers.cpp:23-31 - Always left-aligned
void drawHeaderLeft(Gdey029T94* gfx, const char* title, int x, int y, ...) {
    if (title && title[0] != '\0') {
        gfx->setCursor(x, y);  // Fixed left position
        gfx->print(title);
    }
    gfx->drawFastHLine(0, y + underlineOffset, width, EPD_BLACK);  // Always starts at 0
}

// ListView.cpp:237 - Fixed left alignment
int textX = ITEM_PADDING_X;  // Always from left
if (item.icon) {
    gfx->setCursor(textX, y + 4);
    gfx->print(iconStr);
    textX += 10;
}
gfx->setCursor(textX, y + 4);
gfx->print(item.label);  // Left-to-right only
```

I18n.h (lines 11-14) only defines:
```cpp
enum class Language : uint8_t {
    EN = 0,     // English (default/fallback)
    DE = 1,     // German
    COUNT
};
```

No RTL language support or direction tracking.

## Recommended Fix
**Short-term (if RTL needed soon):**
1. Add `TextDirection` enum to `I18n.h`:
   ```cpp
   enum class TextDirection { LTR, RTL };
   ```

2. Add direction property to `Language` enum or map:
   ```cpp
   TextDirection getLanguageDirection(Language lang);
   ```

3. Modify `drawHeaderLeft()` to accept direction:
   ```cpp
   void drawHeaderLeft(Gdey029T94* gfx, const char* title, int x, int y, 
                       uint16_t width, TextDirection dir = TextDirection::LTR);
   ```

**Long-term:**
- Create a `GridLayout` helper that calculates positions based on direction
- Use logical positioning (start/end) instead of physical (left/right)
- Add RTL testing with sample Arabic/Hebrew strings

## References
- [Unicode Bidirectional Algorithm](https://www.w3.org/International/articles/inline-bidi-markup/)
- [ESP32 Display Libraries RTL Support](https://github.com/adafruit/Adafruit_GFX)
