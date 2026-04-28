---
title: "[MEDIUM] Inconsistent E-Paper Refresh Timing Without Visual Feedback"
severity: MEDIUM
domain: interaction-design
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
E-Paper display refresh operations (especially full refresh) take 350ms-1000ms to complete, but the UI provides no visual feedback during this time. Users may think the device is frozen or unresponsive while waiting for the display to update.

**Files affected:**
- `components/cdc_hal/src/EpaperDisplay.cpp:279-298` - `flush()` with async render task
- `components/cdc_hal/src/EpaperDisplay.cpp:305-313` - `flushSync()` blocking call
- `components/cdc_os_ui/src/views/LockScreenView.cpp:575` - `renderDeepSleepScreen()` with `flushSync()`

## Impact
- **User Experience:** No indication that work is happening during long refresh cycles
- **Perceived Performance:** Full refresh (~1000ms) feels like a "freeze" to users
- **Input Confusion:** Users may press keys repeatedly thinking nothing happened
- **Animation Gap:** No progress indicator or loading state during display updates

## Evidence
```cpp
// EpaperDisplay.cpp:305-313 - Synchronous refresh (blocking)
void EpaperDisplay::flushSync(RefreshMode mode) {
    if (!s_epd_display) return;
    if (mode == RefreshMode::FULL) {
        s_epd_display->update();  // BLOCKS for ~1000ms!
    } else {
        s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);  // ~350ms
    }
}

// LockScreenView.cpp:575 - Deep sleep uses blocking sync refresh
void LockScreenView::renderDeepSleepScreen() {
    deepSleepMode_ = true;
    setClock("");
    setDate("");
    statusIcons_ = StatusIcon::NONE;
    render(false);

    auto* display = hal::getDisplayInstance();
    if (display) {
        display->flushSync(hal::RefreshMode::PARTIAL);  // Blocks UI for ~350ms
    }
}
```

**Timing characteristics:**
- Partial refresh: ~350ms (visible delay)
- Full refresh: ~500-1000ms (significant delay)
- No visual feedback during either operation

## Recommended Fix
Add visual feedback during long refresh cycles:

1. **Add a "processing" state indicator:**
   ```cpp
   void LockScreenView::renderDeepSleepScreen() {
       deepSleepMode_ = true;
       setClock("");
       setDate("");
       statusIcons_ = StatusIcon::NONE;
       
       // Show "Processing..." or spinner before flush
       setInfo("Updating display...");
       render(false);
       
       auto* display = hal::getDisplayInstance();
       if (display) {
           display->flushSync(hal::RefreshMode::PARTIAL);
       }
       
       // Clear info line after refresh
       setInfo("");
   }
   ```

2. **For async flush, show progress indicator:**
   ```cpp
   // In ViewStack::render()
   void ViewStack::render() {
       IView* view = current();
       if (!view) return;
       
       // Show loading indicator if full refresh needed
       if (needsFullRefresh_) {
           view->render(false);
           // Could add a small spinner or "Updating..." text
       }
       
       hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
       hal::IDisplay* display = hal::getDisplayInstance();
       if (display) {
           display->flush(mode);
       }
       needsFullRefresh_ = false;
   }
   ```

3. **Add configurable refresh feedback:**
   ```cpp
   // In IDisplay interface
   virtual void setRefreshIndicator(bool show) = 0;
   ```

4. **Consider using partial refresh first, then full refresh:**
   ```cpp
   // Show immediate partial update
   display->flushSync(RefreshMode::PARTIAL);
   // Schedule full refresh in background
   ```

**Estimated time: 45 minutes**

## References
- E-Paper display characteristics: Partial ~350ms, Full ~500-1000ms
- UX best practices: Show progress for operations >200ms
- Related issue: Deep sleep ghosting (006-deep-sleep-ghosting.md)

</content>