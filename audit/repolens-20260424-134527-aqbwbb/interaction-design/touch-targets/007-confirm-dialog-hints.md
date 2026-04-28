---
title: "[LOW] Confirmation dialog Y/N hints lack visual distinction"
severity: LOW
domain: touch-targets
lens: interaction-design/touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The `ConfirmView` displays Y/N hints at the bottom of the dialog as plain text ("Y=Ja  N=Nein") without visual distinction between the two options. Both options appear with the same visual weight, which may make it less obvious which key to press for confirmation vs. cancellation.

**Evidence:**
- File: `components/cdc_views/include/cdc_views/ConfirmView.h:57-58`
  ```cpp
  const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }
  ```
- File: `components/cdc_views/src/ConfirmView.cpp:183-185`
  ```cpp
  // Draw Y/N hint at bottom
  gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
  gfx->print("Y=Ja  N=Nein");
  ```

The hints are rendered as plain text with no:
- Different colors (E-Paper is B/W, but could use bold/regular distinction)
- Visual separation (like a dividing line)
- Highlighting of the primary action

## Impact
**Decision Clarity:** Without visual distinction:
1. Users may need to read carefully to understand which key confirms vs. cancels
2. The primary action (Y/confirm) is not visually emphasized
3. Users with cognitive load (e.g., confirming an important action) may hesitate

**Consistency:** Other views use the footer bar (black background, white text) for hints, but the confirm dialog uses plain text on white background, creating inconsistency.

## Recommended Fix
Add visual distinction to the Y/N hints:

**Option 1: Bold the primary action**
```cpp
// components/cdc_views/src/ConfirmView.cpp:183-185
gfx->setTextSize(1);
gfx->setTextColor(EPD_BLACK);
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);

// Draw Y hint (primary action) with more emphasis
gfx->print("Y=");
gfx->setTextSize(2);  // Larger for primary action
gfx->print("Ja ");
gfx->setTextSize(1);
gfx->print("  N=Nein");
```

**Option 2: Use boxed hints**
```cpp
// Draw Y in a filled box (primary)
gfx->fillRect(boxX + 40, boxY + BOX_HEIGHT - 14, 20, 12, EPD_BLACK);
gfx->setTextColor(EPD_WHITE);
gfx->setCursor(boxX + 42, boxY + BOX_HEIGHT - 12);
gfx->print("Y");

// Draw N in outline box (secondary)
gfx->drawRect(boxX + 70, boxY + BOX_HEIGHT - 14, 20, 12, EPD_BLACK);
gfx->setTextColor(EPD_BLACK);
gfx->setCursor(boxX + 72, boxY + BOX_HEIGHT - 12);
gfx->print("N");
```

**Option 3: Use the standard footer bar style**
```cpp
// Move hints to a small footer-like bar at bottom of dialog
gfx->fillRect(boxX + 5, boxY + BOX_HEIGHT - 16, BOX_WIDTH - 10, 14, EPD_BLACK);
gfx->setTextColor(EPD_WHITE);
gfx->setCursor(boxX + 10, boxY + BOX_HEIGHT - 14);
gfx->print("Y=Ja  N=Nein");
```

This would provide:
- Clearer visual hierarchy between confirm and cancel
- Better distinction for the primary action
- More consistent styling with the rest of the UI

## References
- Nielsen Norman Group: Primary actions should be visually distinct from secondary actions
- Material Design: Primary buttons use filled style, secondary use outlined
- Visual hierarchy: Important choices benefit from clear visual emphasis
