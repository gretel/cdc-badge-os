---
title: "[MEDIUM] Missing visual feedback during long-press interactions"
severity: MEDIUM
domain: interaction-design
lens: interactive-feedback
labels:
  - "audit:interaction-design/interactive-feedback"
---

## Summary
Most view components in the UI framework do not provide visual feedback during long-press interactions. Only `T9InputView` implements `onLongPress()` (at `components/cdc_views/src/T9InputView.cpp:238`), but other interactive views like `SliderView`, `ListView`, `DateInputView`, `RgbInputView`, and `PinEntryView` lack long-press handlers.

### Affected Components:
- `SliderView` (`components/cdc_views/src/SliderView.cpp`) - Long-press on [4]/[6] could speed up adjustment
- `ListView` (`components/cdc_views/src/ListView.cpp`) - Long-press could enable faster scrolling
- `DateInputView` (`components/cdc_views/src/DateInputView.cpp`) - Long-press could clear field faster
- `RgbInputView` (`components/grove_led/src/RgbInputView.cpp`) - Long-press could cycle fields faster
- `ContextMenuView` (`components/cdc_views/src/ContextMenuView.cpp`) - Long-press could enable faster navigation

## Impact
**User Experience**: Users have no visual confirmation that a long-press is being registered. On an E-Paper display (which has slower refresh rates), this is particularly important to prevent repeated pressing.

**Discoverability**: Long-press features are hidden without visual indication. Users may not know long-press is available.

**Efficiency**: Without long-press feedback, users might manually tap repeatedly instead of holding for faster operation.

## Evidence
1. `T9InputView` is the only view with long-press support:
   - `components/cdc_views/src/T9InputView.cpp:238-258` - `onLongPress(char key)` handles 'N' for clear-all and digits for force-insert

2. `SliderView.adjust()` at line 71-91 could benefit from long-press but has no handler:
   ```cpp
   void SliderView::adjust(bool increase) {
       // Single-step adjustment only
       uint16_t newValue = value_;
       uint16_t currentStep = stepCallback_ ? stepCallback_(value_, increase) : step_;
       // ...
   }
   ```

3. Keypad long-press detection exists in `TCA9535Keypad` (`components/cdc_hal/src/TCA9535Keypad.cpp:328-348`) with 800ms threshold, but most views don't consume these events.

## Recommended Fix
1. **Add long-press handlers to frequently-used views**:
   - `SliderView`: Implement `onLongPress()` to enable continuous adjustment (e.g., auto-repeat every 100ms while held)
   - `ListView`: Implement `onLongPress()` for faster scrolling (e.g., jump 3-5 items at a time)
   - `DateInputView`/`RgbInputView`: Add long-press to clear field or cycle faster

2. **Provide visual feedback during long-press**:
   - Show a small indicator (e.g., "holding..." text or icon) after 500ms
   - Update the display periodically during long-press to show progress

3. **Document long-press features**:
   - Add to footer hints (e.g., "N=Back (hold=clear)")
   - Consider adding a help screen that lists long-press shortcuts

### Implementation Steps (1-hour scope):
1. Add `onLongPress(char key)` to `SliderView` (15 min)
2. Add visual feedback indicator during long-press (15 min)
3. Update footer hint to document long-press (10 min)
4. Test on hardware (10 min)
5. Repeat for 1-2 other high-impact views (10 min)

## References
- ESP32 keypad long-press detection: `components/cdc_hal/src/TCA9535Keypad.cpp:328-348`
- T9InputView long-press implementation: `components/cdc_views/src/T9InputView.cpp:238-258`
- IView interface (base for long-press): `components/cdc_ui/include/cdc_ui/IView.h:76-80`
