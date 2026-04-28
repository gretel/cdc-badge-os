---
title: "[LOW] Blocking vTaskDelay After Display Render in Sleep Flow"
severity: LOW
domain: interaction-design
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
The sleep manager uses `vTaskDelay(350)` after rendering to wait for E-Paper partial refresh to complete. While this is intentional (to ensure the display settles before sleep), it blocks the UI task for 350ms with no visual feedback. This pattern occurs multiple times in the sleep/wake flow.

**Location:** `components/cdc_os_ui/src/SleepManager.cpp:118-120` and `:173`

## Impact
- **UI Responsiveness:** UI is frozen for 350ms during sleep entry and wake cycles
- **Visual Feedback:** No indication that the display is being updated
- **Consistency:** Multiple blocking delays throughout sleep flow (at least 2 occurrences)

## Evidence
```cpp
// SleepManager.cpp:118-120 - Before entering light sleep
void SleepManager::enterLockScreenSleep() {
    // Show light sleep icon
    lockScreen_->addStatusIcon(StatusIcon::LIGHT_SLEEP);
    inLightSleep_ = true;

    // Render the icon before sleeping
    ViewStack::instance().render();

    // Wait for E-Paper partial refresh to complete
    vTaskDelay(pdMS_TO_TICKS(350));  // BLOCKS for 350ms

    // Enter light sleep (blocking call, returns after wakeup)
    sleep_->enterLightSleep();
}

// SleepManager.cpp:173 - After rendering clock update on wakeup
void SleepManager::handleWakeup() {
    // ... update clock ...
    
    // Render clock update
    ViewStack::instance().render();
    vTaskDelay(pdMS_TO_TICKS(350));  // BLOCKS for 350ms
    
    // Check if USB was connected during sleep
    if (power_ && power_->isUsbConnected()) {
        // ...
    } else {
        // Go back to sleep
        sleep_->enterLightSleep();
    }
}
```

**Comment indicates awareness:**
```cpp
// Wait for E-Paper partial refresh to complete
vTaskDelay(pdMS_TO_TICKS(350));
```

The code shows awareness of E-Paper timing characteristics but uses a blocking delay.

## Recommended Fix
While the blocking delay may be intentional (to ensure display settles before sleep), consider these alternatives:

1. **Use async flush with notification:**
   ```cpp
   void SleepManager::enterLockScreenSleep() {
       lockScreen_->addStatusIcon(StatusIcon::LIGHT_SLEEP);
       inLightSleep_ = true;
       ViewStack::instance().render();
       
       // Use async flush instead of blocking delay
       auto* display = hal::getDisplayInstance();
       if (display) {
           display->flush(hal::RefreshMode::PARTIAL);
           // Could wait on semaphore instead of blocking
       }
       
       sleep_->enterLightSleep();
   }
   ```

2. **If blocking is necessary, document the reason clearly:**
   ```cpp
   // Wait for E-Paper partial refresh to complete (~350ms)
   // Blocking is intentional here to ensure display settles before sleep
   vTaskDelay(pdMS_TO_TICKS(350));
   ```

3. **Consider using a shorter delay with display busy check:**
   ```cpp
   vTaskDelay(pdMS_TO_TICKS(100));  // Minimum settle time
   while (display->isBusy()) {
       vTaskDelay(pdMS_TO_TICKS(10));
   }
   ```

**Estimated time: 30 minutes**

## References
- E-Paper displays: Partial refresh typically 350ms
- SleepManager.cpp:118 comment shows awareness of refresh timing
- Similar blocking pattern in `LockScreenView::checkDeepSleepTrigger()` (see 007-blocking-delay-deep-sleep.md)

</content>