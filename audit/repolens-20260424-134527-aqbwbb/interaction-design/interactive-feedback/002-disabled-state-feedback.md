---
title: "[MEDIUM] No visual feedback for disabled/locked-out state in PIN entry"
severity: MEDIUM
domain: interaction-design
lens: interactive-feedback
labels:
  - "interactive-feedback"
---

## Summary
When the PIN entry is locked out (after failed attempts), the keypad input is disabled but there's no visual indication that the input fields are "disabled" - the dots still appear interactive and the user might continue pressing keys expecting feedback.

**Location:** `components/cdc_views/src/PinEntryView.cpp:180-210`

The `onKey()` method returns `IGNORED` when locked out, but the visual state doesn't clearly communicate this disabled state.

## Impact
- **Unclear state**: Users may not understand why key presses aren't working
- **Continued interaction**: Users might keep pressing keys expecting some response
- **Accessibility**: No clear visual cue for the "disabled" state beyond the text message

## Evidence
```cpp
// components/cdc_views/src/PinEntryView.cpp:180-185
InputResult PinEntryView::onKey(char key) {
    if (lockedOut_) {
        return InputResult::IGNORED;  // Keys are ignored but UI looks the same
    }
    // ...
}

// components/cdc_views/src/PinEntryView.cpp:255-275
// PIN dots are rendered the same way regardless of locked-out state
for (uint8_t i = 0; i < maxLength_; i++) {
    int x = startX + i * PIN_DOT_SPACING;
    int y = PIN_Y;

    if (i < length_) {
        // Filled dot for entered digits
        gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
    } else {
        // Empty dot for remaining positions
        gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
    }
}
```

**Current behavior:**
- When `lockedOut_` is true, key presses are silently ignored
- The PIN dots (filled and empty) look identical to the enabled state
- The footer hint doesn't change to indicate the disabled state
- Only the "LOCKED_OUT: Xs" text indicates the state, which might be missed

## Recommended Fix
Add visual indicators for the disabled/locked-out state:

1. **Dim or grey out the PIN dots** when locked out:
   ```cpp
   // In render(), when lockedOut_, use a lighter visual style
   if (lockedOut_) {
       // Draw dots with reduced contrast (e.g., grey instead of black)
       // For e-paper, could use lighter fill or different pattern
       gfx->drawCircle(..., EPD_BLACK);  // Still visible but less prominent
   }
   ```

2. **Update the footer hint** to clearly indicate disabled state:
   ```cpp
   const char* PinEntryView::getFooterHint() const {
       if (lockedOut_) {
           return tr(StringId::LOCKED_OUT_HINT);  // e.g., "Locked... press any key to retry"
       }
       return tr(StringId::HINT_PIN_INPUT);
   }
   ```

3. **Add a visual overlay or indicator** (optional):
   - Draw a diagonal line through the PIN area
   - Add a lock icon near the dots

**Scope:** ~1 hour implementation
- Modify `render()` to check `lockedOut_` and adjust visual style
- Add new localized string for locked-out footer hint
- Test visual appearance on actual display

## References
- WCAG 2.1: Understanding disabled state (1.4.1 Use of Color)
- Existing MessageBox overlay for error feedback in same file
