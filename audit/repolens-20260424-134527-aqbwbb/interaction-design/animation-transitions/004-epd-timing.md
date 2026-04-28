---
title: "[MEDIUM] E-Paper Refresh Timing Not Optimized for Transitions"
severity: MEDIUM
domain: animation-transitions
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
E-Paper display refresh timing uses fixed delays (350ms) without considering the actual refresh mode or display state. The delay is applied uniformly for both partial refresh and light sleep transitions, which may be suboptimal.

**Files affected:**
- `components/cdc_hal/src/EpaperDisplay.cpp:110-117` - `renderTask()` with `updateWindow()`
- `components/cdc_os_ui/src/SleepManager.cpp:112` - `vTaskDelay(pdMS_TO_TICKS(350))`
- `components/cdc_os_ui/src/SleepManager.cpp:168` - `vTaskDelay(pdMS_TO_TICKS(350))`
- `components/cdc_ui/src/ViewStack.cpp:264-268` - `render()` with FULL/PARTIAL mode

## Impact
- **Performance:** Fixed 350ms delay may be too long for some cases, too short for others
- **User Experience:** Slower transitions than necessary
- **Power:** Longer delays keep display active longer
- **Ghosting:** May not wait long enough for partial refresh to complete, causing ghosting

## Evidence
```cpp
// SleepManager.cpp:112
vTaskDelay(pdMS_TO_TICKS(350));  // Fixed delay before sleep

// EpaperDisplay.cpp:110-117
if (doFull) {
    s_epd_display->update();  // No wait for completion
} else {
    s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);  // No wait
}

// ViewStack.cpp:264-268
hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
hal::IDisplay* display = hal::getDisplayInstance();
if (display) {
    display->flush(mode);  // Async, but no timing guarantee
}
```

## Recommended Fix
1. **Add dynamic timing based on refresh mode:**
   ```cpp
   // In EpaperDisplay.cpp
   void EpaperDisplay::flush(RefreshMode mode) {
       uint16_t delay = (mode == RefreshMode::FULL) ? 500 : 350;
       // Use configurable timing
   }
   ```

2. **Add display busy check:**
   ```cpp
   // Wait for display to be ready before sleep
   while (display->isBusy()) {
       vTaskDelay(pdMS_TO_TICKS(10));
   }
   ```

3. **Document the timing rationale:**
   - Why 350ms for partial refresh?
   - Why 500ms for full refresh?
   - Temperature considerations (E-Paper is slower in cold)

## References
- E-Paper display technical specifications
- CalEPD library documentation for refresh timing
