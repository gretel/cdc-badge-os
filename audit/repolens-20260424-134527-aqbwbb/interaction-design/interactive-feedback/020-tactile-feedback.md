---
title: "[LOW] No tactile/audio feedback option for key presses"
severity: LOW
domain: interaction-design
lens: interactive-feedback
labels:
  - "audit:interaction-design/interactive-feedback"
---

## Summary
The UI relies solely on visual feedback for key presses. On an E-Paper display with slow refresh rates, adding tactile (vibration) or audio (click) feedback would significantly improve the perceived responsiveness of the interface.

### Current State:
- Only visual feedback via display refresh (200-500ms latency)
- No haptic feedback (vibration motor if available)
- No audio feedback (click sound if speaker available)

## Impact
**Perceived Responsiveness**: A 50ms vibration feels instant compared to 300ms visual update.

**Usability in Low Light**: Users can operate the device without looking at the display.

**Confirmation**: Tactile feedback confirms key press even before the display updates.

## Evidence
1. **Keypad hardware** (`components/cdc_hal/src/TCA9535Keypad.cpp`):
   - 12-button mechanical keypad with tactile switches
   - Physical keypress is already ~10ms responsive
   - But no additional feedback beyond the mechanical click

2. **E-Paper display** (`components/cdc_hal/src/EpaperDisplay.cpp`):
   - Typical refresh time: 200-500ms
   - Partial refresh faster but still noticeable delay
   - User presses key → waits 300ms → sees change

3. **No feedback hooks in view system**:
   - `IView::onKey()` only returns `InputResult`
   - No callback for "play feedback sound" or "vibrate"
   - `ViewStack::dispatchKey()` doesn't trigger feedback

## Recommended Fix
1. **Add feedback hooks to IView**:
   ```cpp
   virtual void onKeyFeedback(char key) {
       // Default: no feedback
   }
   ```

2. **Implement feedback in HAL**:
   - `IHaptics` interface (if vibration motor exists)
   - `IAudio` interface (if speaker exists)
   - Called from `ViewStack::dispatchKey()` after `onKey()`

3. **Add configuration option**:
   - Enable/disable feedback in settings
   - Adjust feedback intensity/duration

### Implementation Steps (1-hour scope):
1. Add `IHaptics` interface to `cdc_hal` (20 min)
2. Add `onKeyFeedback()` to `IView` (10 min)
3. Call feedback from `ViewStack::dispatchKey()` (15 min)
4. Create simple implementation (buzz for 50ms) (15 min)

## References
- Keypad hardware: `components/cdc_hal/src/TCA9535Keypad.cpp`
- IView interface: `components/cdc_ui/include/cdc_ui/IView.h`
- ViewStack dispatch: `components/cdc_ui/include/cdc_ui/ViewStack.h`
