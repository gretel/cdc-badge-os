---
title: "[LOW] ConfirmView lacks visual distinction between Y/N actions"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "action-states"
  - "confirmation-dialog"
  - "key-mapping"
---

## Summary
The `ConfirmView` component (`components/cdc_views/src/ConfirmView.cpp:180-187`) shows a Y/N confirmation dialog with the hint "Y=Ja N=Nein" but provides no visual indication of which key corresponds to which action. The two buttons (confirm vs cancel) have equal visual weight without any visual cue to distinguish them.

## Impact
- **Action clarity**: Users must remember which key confirms and which cancels
- **Error prevention**: No visual hierarchy to suggest the "safer" action (usually cancel)
- **Accessibility**: Users with cognitive load may have difficulty remembering the key mapping

## Evidence
File: `components/cdc_views/src/ConfirmView.cpp`, lines 180-187

```cpp
// Draw Y/N hint at bottom
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print("Y=Ja  N=Nein");
```

The hint text is rendered as plain text with no visual differentiation:
- Both Y and N have the same font size and weight
- No icons or symbols to distinguish confirm vs cancel
- No color or emphasis to suggest primary vs secondary action

## Recommended Fix
Add visual distinction between the two actions:

**Option A - Use icons**:
```cpp
// Draw checkmark for Y
gfx->setCursor(boxX + BOX_WIDTH / 2 - 50, boxY + BOX_HEIGHT - 12);
gfx->print("[");
gfx->fillRect(boxX + BOX_WIDTH / 2 - 42, boxY + BOX_HEIGHT - 10, 8, 8, EPD_BLACK);
gfx->setTextColor(EPD_WHITE);
gfx->print("Y");
gfx->setTextColor(EPD_BLACK);
gfx->print("] ");

// Draw X for N
gfx->print("[N] ");
```

**Option B - Emphasize the primary action**:
```cpp
// Make Y (confirm) more prominent
gfx->setTextSize(2);  // Larger for primary action
gfx->setCursor(boxX + BOX_WIDTH / 2 - 40, boxY + BOX_HEIGHT - 12);
gfx->print("Y=Ja");
gfx->setTextSize(1);  // Back to normal for secondary
gfx->print("  N=Nein");
```

**Option C - Use different text styling**:
```cpp
// Draw confirm with emphasis
gfx->setCursor(boxX + BOX_WIDTH / 2 - 50, boxY + BOX_HEIGHT - 12);
gfx->print("[");
gfx->fillRect(boxX + BOX_WIDTH / 2 - 40, boxY + BOX_HEIGHT - 10, 10, 10, EPD_BLACK);
gfx->setTextColor(EPD_WHITE);
gfx->print("Y");
gfx->setTextColor(EPD_BLACK);
gfx->print("]=Ja  ");

// Draw cancel normally
gfx->print("[N]=Nein");
```

**Option D - Add directional hints**:
```cpp
// Y on left (confirm = forward), N on right (cancel = back)
gfx->setCursor(boxX + BOX_WIDTH / 2 - 50, boxY + BOX_HEIGHT - 12);
gfx->print("Y=Ja [OK]");
gfx->setCursor(boxX + BOX_WIDTH / 2 + 20, boxY + BOX_HEIGHT - 12);
gfx->print("[Back] N=Nein");
```

## References
- [Button hierarchy design patterns](https://www.nngroup.com/articles/button-hierarchy/)
- [Confirmation dialog best practices](https://www.w3.org/WAI/tutorials/modals/)
