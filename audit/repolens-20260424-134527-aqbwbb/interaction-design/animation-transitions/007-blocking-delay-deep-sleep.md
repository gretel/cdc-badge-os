---
title: "[MEDIUM] Blocking vTaskDelay in UI Transition Flow"
severity: MEDIUM
domain: interaction-design
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
The deep sleep transition in `LockScreenView::checkDeepSleepTrigger()` uses a blocking `vTaskDelay(2000)` after rendering the deep sleep screen. This blocks the entire UI thread for 2 seconds, preventing any further UI updates or input processing during this period.

**Location:** `components/cdc_os_ui/src/views/LockScreenView.cpp:376-378`

## Impact
- **UI Responsiveness:** The UI is completely frozen for 2 seconds during a critical transition
- **Input Handling:** Any key presses during the delay are lost
- **Animation Quality:** No opportunity for visual feedback or smooth transition animations
- **Maintainability:** Blocking delays are harder to manage than non-blocking state machines

## Evidence
```cpp
// LockScreenView.cpp:370-390
void LockScreenView::checkDeepSleepTrigger(uint32_t nowMs) {
    auto* keypad = hal::getKeypadInstance();
    if (!keypad) return;

    bool nPressed = keypad->isKeyPressed(hal::Key::KEY_NO);

    if (nPressed) {
        if (nPressStartMs_ == 0) {
            nPressStartMs_ = nowMs;
        } else {
            uint32_t elapsed = nowMs - nPressStartMs_;
            if (elapsed >= DEEP_SLEEP_HOLD_MS) {
                LOG_I(TAG, "Long-press N detected, entering deep sleep...");

                // Render deep sleep screen and push to e-paper
                renderDeepSleepScreen();

                // Turn off backlight
                auto* display = hal::getDisplayInstance();
                if (display) {
                    display->backlightOff();
                }

                // Wait 2s so user can release button without triggering wakeup
                vTaskDelay(pdMS_TO_TICKS(2000));  // BLOCKING for 2 seconds!

                // Enter deep sleep (does not return)
                auto* sleep = hal::getSleepControllerInstance();
                if (sleep) {
                    sleep->enterDeepSleep();
                }
            }
        }
    } else {
        nPressStartMs_ = 0;
    }
}
```

The comment explains the intent: "Wait 2s so user can release button without triggering wakeup". However, using `vTaskDelay()` blocks the entire task, preventing:
- UI tick updates
- Input polling
- Other view lifecycle methods

## Recommended Fix
Replace the blocking delay with a non-blocking state machine approach:

1. **Add a state variable for the wait period:**
   ```cpp
   enum class DeepSleepState {
       INACTIVE,
       READY_TO_SLEEP,
       WAITING_FOR_RELEASE
   };
   
   static DeepSleepState s_deepSleepState = DeepSleepState::INACTIVE;
   static uint32_t s_sleepWaitStartMs = 0;
   ```

2. **Update the tick handler to manage the wait:**
   ```cpp
   void LockScreenView::checkDeepSleepTrigger(uint32_t nowMs) {
       auto* keypad = hal::getKeypadInstance();
       if (!keypad) return;

       bool nPressed = keypad->isKeyPressed(hal::Key::KEY_NO);

       switch (s_deepSleepState) {
           case DeepSleepState::INACTIVE:
               if (nPressed) {
                   if (s_nPressStartMs_ == 0) {
                       s_nPressStartMs_ = nowMs;
                   } else if (nowMs - s_nPressStartMs_ >= DEEP_SLEEP_HOLD_MS) {
                       // Trigger deep sleep sequence
                       renderDeepSleepScreen();
                       hal::getDisplayInstance()->backlightOff();
                       s_deepSleepState = DeepSleepState::WAITING_FOR_RELEASE;
                       s_sleepWaitStartMs = nowMs;
                   }
               } else {
                   s_nPressStartMs_ = 0;
               }
               break;

           case DeepSleepState::WAITING_FOR_RELEASE:
               if (!nPressed && (nowMs - s_sleepWaitStartMs >= 2000)) {
                   // Button released and 2s passed, enter deep sleep
                   hal::getSleepControllerInstance()->enterDeepSleep();
                   s_deepSleepState = DeepSleepState::INACTIVE;
               }
               break;
       }
   }
   ```

3. **Benefits:**
   - UI remains responsive during the 2-second wait
   - Can add visual countdown feedback
   - Easier to test and maintain
   - Follows the existing non-blocking pattern in `onTick()`

**Estimated time: 45 minutes**

## References
- ESP32 FreeRTOS patterns: Non-blocking state machines preferred over `vTaskDelay()`
- UI responsiveness guidelines: Keep UI thread free for input and animations
- Similar pattern already used in `ToastView::onTick()` and `MessageBox::onTick()`

</content>