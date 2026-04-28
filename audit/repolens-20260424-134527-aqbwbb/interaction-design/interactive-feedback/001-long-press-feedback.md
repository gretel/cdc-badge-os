---
title: "[MEDIUM] Missing visual feedback for long-press interactions"
severity: MEDIUM
domain: interaction-design
lens: interactive-feedback
labels:
  - "interactive-feedback"
---

## Summary
The lock screen implements a long-press interaction (hold N key for 5 seconds to enter deep sleep) but provides no visual countdown feedback to indicate the press duration or remaining time until activation.

**Location:** `components/cdc_os_ui/src/views/LockScreenView.cpp:355-397`

The `checkDeepSleepTrigger()` function tracks the press duration but only acts when the threshold is reached, without updating the display to show progress.

## Impact
- **User confusion**: Users holding the N key have no indication whether their press is being registered or how long they need to hold it
- **Interaction uncertainty**: Without feedback, users may release too early or wonder if the system is responsive
- **Discoverability**: The long-press feature is not intuitively discoverable without prior knowledge

## Evidence
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:355-397
void LockScreenView::checkDeepSleepTrigger(uint32_t nowMs) {
    auto* keypad = hal::getKeypadInstance();
    if (!keypad) return;

    bool nPressed = keypad->isKeyPressed(hal::Key::KEY_NO);

    if (nPressed) {
        if (nPressStartMs_ == 0) {
            // N key just pressed, start timing
            nPressStartMs_ = nowMs;
        } else {
            // Check if held long enough
            uint32_t elapsed = nowMs - nPressStartMs_;
            if (elapsed >= DEEP_SLEEP_HOLD_MS) {
                LOG_I(TAG, "Long-press N detected, entering deep sleep...");

                // Render deep sleep screen and push to e-paper
                renderDeepSleepScreen();
                // ... enters deep sleep
            }
        }
    } else {
        // N released, reset timer
        nPressStartMs_ = 0;
    }
}
```

**Current behavior:**
- `onTick()` is called periodically (line 341)
- `checkDeepSleepTrigger()` calculates elapsed time (line 366)
- No visual update is triggered until the threshold is reached
- The footer hint remains static ("PRESS ANY KEY" or "DEEP_SLEEP" only after threshold)

## Recommended Fix
Add visual countdown feedback during the long-press:

1. **Update the footer hint dynamically** to show elapsed time or a countdown:
   ```cpp
   // In checkDeepSleepTrigger(), when nPressed and elapsed > 0:
   if (nPressed && nPressStartMs_ > 0) {
       uint32_t elapsed = nowMs - nPressStartMs_;
       if (elapsed >= DEEP_SLEEP_HOLD_MS) {
           // Trigger deep sleep
       } else {
           // Update footer with countdown (e.g., "Hold N for deep sleep: 3s")
           char hint[32];
           uint32_t remaining = (DEEP_SLEEP_HOLD_MS - elapsed) / 1000 + 1;
           snprintf(hint, sizeof(hint), "Hold N for deep sleep: %lus", remaining);
           // Trigger re-render with new hint
           dirty_ = true;
       }
   }
   ```

2. **Add a visual progress indicator** (optional enhancement):
   - Draw a simple bar or dot sequence that fills as the press continues
   - Update during each tick while the key is held

3. **Show immediate feedback** when the key is first pressed:
   - Change footer from "PRESS ANY KEY" to "Hold N for deep sleep..."
   - This confirms the key press is registered

**Scope:** ~1 hour implementation
- Modify `checkDeepSleepTrigger()` to update `dirty_` during press
- Update `getFooterHint()` to return dynamic text when counting down
- Test with varying press durations

## References
- ESP32-S3 Badge keypad interaction patterns
- E-Paper display refresh considerations (partial vs full updates)
- Existing `onTick()` pattern used in `PinEntryView` for lockout countdown
