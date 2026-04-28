---
title: "[LOW] No visual indication of field focus state in input views - RgbInputView"
severity: LOW
domain: interaction-design
lens: interactive-feedback
labels:
  - "audit:interaction-design/interactive-feedback"
---

## Summary
Input views with multiple fields show an underline to indicate the active field, but the underline styling is identical whether the field is being edited or just selected. There is no distinction between "field selected" and "field being edited" states.

**Note**: See also `012-dateinput-field-focus.md` for DateInputView/TimeInputView-specific details.

### Current Behavior:
- `DateInputView`: Underline moves to current field (day/month/year) - see 012
- `RgbInputView`: Underline moves to current field (R/G/B) - **focus here**
- No visual difference between "just navigated to field" and "typing in field"

## Impact
**Clarity**: Users may be confused about which field will receive input, especially after using navigation keys.

**Accessibility**: Users with visual impairments may have difficulty tracking which field is active.

**E-Paper Consideration**: E-Paper displays have lower contrast than LCD, making subtle indicators harder to see.

## Evidence
1. **DateInputView** (`components/cdc_views/src/DateInputView.cpp:242-262`):
   ```cpp
   // Clear old underline
   gfx->fillRect(0, UNDERLINE_Y, width, 4, EPD_WHITE);

   // Draw underline for current field
   int charWidth = 12;
   int underlineX = startX;
   int underlineW = 0;

   switch (currentField_) {
       case Field::DAY:
           underlineX = startX;
           underlineW = 2 * charWidth;
           break;
       case Field::MONTH:
           underlineX = startX + 5 * charWidth;
           underlineW = 2 * charWidth;
           break;
       case Field::YEAR:
           underlineX = startX + 10 * charWidth;
           underlineW = 4 * charWidth;
           break;
   }

   gfx->fillRect(underlineX, UNDERLINE_Y, underlineW, 3, EPD_BLACK);
   ```
   The underline is static - no indication of whether the field is "active for editing" vs "just selected".

2. **RgbInputView** (`components/grove_led/src/RgbInputView.cpp:222-242`):
   Similar pattern - underline shows current field but no additional feedback for edit state.

3. **T9InputView** (`components/cdc_views/src/T9InputView.cpp:298-320`):
   ```cpp
   // Show cursor with inverted text for active character
   if (cursorActive_ && i == len_ - 1) {
       int16_t x = gfx->getCursorX();
       int16_t y = gfx->getCursorY();
       gfx->fillRect(x, y - 2, 8, 14, EPD_BLACK);  // Inverted background
       gfx->setTextColor(EPD_WHITE);
       gfx->print(text_[i]);
       gfx->setTextColor(EPD_BLACK);
   }
   ```
   T9InputView actually has better feedback - it inverts the current character when actively editing!

## Recommended Fix
1. **Add "editing" visual indicator**:
   - Blinking cursor (toggle every 500ms using `onTick()`)
   - Different underline style (double line, thicker line) when actively editing
   - Invert the current field value (like T9InputView does)

2. **Show edit feedback on key press**:
   - When a digit is entered, briefly highlight that field
   - Return to normal state after 200ms

3. **Consider field transition animation**:
   - Flash the new field when navigation happens
   - Helps user track where focus moved

### Implementation Steps (1-hour scope):
1. Add blinking cursor to `RgbInputView` using `onTick()` (30 min)
2. Test on hardware (15 min)
3. Document pattern for other input views (15 min)

**Note**: DateInputView/TimeInputView implementation is covered in `012-dateinput-field-focus.md`.

## References
- DateInputView underline: `components/cdc_views/src/DateInputView.cpp:242-262`
- RgbInputView underline: `components/grove_led/src/RgbInputView.cpp:222-242`
- T9InputView cursor (good example): `components/cdc_views/src/T9InputView.cpp:298-320`
