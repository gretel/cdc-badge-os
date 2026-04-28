---
title: "[MEDIUM] No immediate visual feedback on key press (press state) - General pattern"
severity: MEDIUM
domain: interaction-design
lens: interactive-feedback
labels:
  - "audit:interaction-design/interactive-feedback"
---

## Summary
When a key is pressed, the UI does not provide immediate visual feedback to confirm the press was registered. On an E-Paper display with ~200-500ms refresh latency, users may wonder if their press was registered and tap multiple times.

**Note**: See also `006-slider-press-feedback.md` for SliderView-specific implementation details.

### Current Behavior
- Key press triggers `onKey()` → view marks `dirty_ = true` → next render cycle updates display
- No intermediate visual state between "idle" and "updated"
- No "pressed" animation or highlight

### Affected Components (beyond SliderView):
- `ListView` - selection changes but no press indicator
- `PinEntryView` - dots appear but no press feedback
- `DateInputView` - field changes but no press indicator
- `T9InputView` - character added but no press feedback
- `RgbInputView` - value changes but no press indicator
- `ContextMenuView` - items navigate but no press highlight
- Lock screen - unlock happens but no press feedback

## Impact
**User Confidence**: Users may tap repeatedly if they don't see immediate feedback, causing confusion or duplicate actions.

**E-Paper Specific**: E-Paper displays have slower refresh rates than LCD/OLED. Without immediate feedback, latency feels higher.

**Accessibility**: Users with motor control issues rely on immediate feedback to know their input was registered.

## Evidence
1. **SliderView** (`components/cdc_views/src/SliderView.cpp:101-119`):
   ```cpp
   InputResult SliderView::onKey(char key) {
       switch (key) {
           case '6': // Right = Increase
               adjust(true);  // No visual feedback here
               return InputResult::CONSUMED;
           case '4': // Left = Decrease
               adjust(false); // No visual feedback here
               return InputResult::CONSUMED;
       }
   }
   ```
   The slider adjusts but doesn't show any press indication before the next render.

2. **ListView** (`components/cdc_views/src/ListView.cpp:138-165`):
   ```cpp
   InputResult ListView::onKey(char key) {
       switch (key) {
           case '2': // Up
               navigate(false); // Marks dirty, but no immediate feedback
               return InputResult::CONSUMED;
       }
   }
   ```
   Selection changes but user sees no indication that the press was registered.

3. **Keypad event flow** (`components/cdc_hal/src/TCA9535Keypad.cpp:395-420`):
   - Key press → ISR → task → callback → view `onKey()`
   - Total latency ~20-50ms before view gets event
   - E-Paper refresh adds 200-500ms more
   - User waits 250-550ms for visual confirmation

## Recommended Fix
1. **Implement immediate press feedback**:
   - Add a small visual indicator that appears instantly on key press
   - Options:
     - Flash the footer bar briefly
     - Show a small "press" icon in corner
     - Invert the selected item briefly

2. **Use partial refresh for faster feedback**:
   - Show press indicator using partial refresh (faster than full refresh)
   - Update full view with complete state in next cycle

3. **Consider haptic/audio feedback** (if hardware supports):
   - Short vibration on key press
   - Click sound (if speaker available)

### Implementation Steps (1-hour scope):
1. Add a `pressFeedback()` method to `IView` base class (10 min)
2. Implement in `ListView` - flash the selected row (20 min)
3. Implement in `PinEntryView` - flash the last dot (15 min)
4. Test on hardware (10 min)
5. Document pattern for other views (5 min)

**Note**: SliderView implementation is covered in `006-slider-press-feedback.md`.

## References
- E-Paper display specs: Good Display GDEY029T94 (typical 200-500ms refresh)
- Keypad polling: `components/cdc_hal/src/TCA9535Keypad.cpp:380-430`
- View rendering: `components/cdc_ui/include/cdc_ui/IView.h:51-59`
