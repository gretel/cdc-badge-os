---
title: "[MEDIUM] Footer bar height of 16px may cause cramped key hint display"
severity: MEDIUM
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The footer bar that displays key hints (e.g., "[Y] OK [N] Back", "[2] Up [8] Down [Y] Select [N] Back") has a height of only **16 pixels** (`kFooterHeight = 16` in `components/cdc_views/include/cdc_views/RenderHelpers.h:9`). This may result in cramped text rendering, especially when multiple key hints need to be displayed.

**Evidence:**
- File: `components/cdc_views/include/cdc_views/RenderHelpers.h:9`
  ```cpp
  constexpr int kFooterHeight = 16;
  ```
- File: `components/cdc_views/src/RenderHelpers.cpp:65-70`
  ```cpp
  void drawFooterBar(Gdey029T94* gfx, uint16_t width, uint16_t height,
                     const char* prefix, const char* hint, bool force) {
      if (!gfx) return;
      if (!force && !prefix && !hint) return;

      gfx->fillRect(0, height - kFooterHeight, width, kFooterHeight, EPD_BLACK);
      gfx->setTextColor(EPD_WHITE);
      gfx->setCursor(4, height - 12);  // Cursor at y=112 for 128px display
  ```

The cursor is placed at `height - 12` (y=116 for a 128px display), which leaves only **4 pixels** of vertical space below the cursor baseline for text descent.

## Impact
**Text Clarity:** With a 16px footer and text rendered at size 1 (6x8 font typically), the text may:
1. Appear cramped vertically
2. Have limited room for proper line spacing
3. Look crowded when multiple hints are shown (e.g., "[2] Up [8] Down [Y] Select [N] Back")

**Readability:** The key hints are critical for user navigation. If they appear cramped or hard to read, users may struggle to understand available actions.

**Display Real Estate:** The footer uses 16/128 = **12.5%** of the screen height. This is reasonable, but the actual usable space for text is less due to cursor positioning.

## Recommended Fix
Increase `kFooterHeight` from **16px to 20px** to provide better text rendering space:

```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h:9
constexpr int kFooterHeight = 20;  // Increased from 16
```

And adjust the cursor position in `drawFooterBar`:
```cpp
// components/cdc_views/src/RenderHelpers.cpp:69
gfx->setCursor(4, height - 14);  // Adjusted from height - 12
```

This would provide:
- More vertical breathing room for footer text
- Better visual separation from the list/content area above
- Improved readability of key hints

If space is at a premium, a minimum of **18px** would still be an improvement over 16px.

## References
- WCAG 1.4.8 Visual Presentation: Text should have sufficient line spacing and visual room
- Material Design: Bottom app bar height is typically 56dp (adapted for small displays: proportionally larger)
- E-Paper displays have lower refresh rates, so clear, readable text is more important to minimize redraws
