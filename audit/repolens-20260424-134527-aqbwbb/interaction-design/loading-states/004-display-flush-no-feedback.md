---
title: "[MEDIUM] Display flush operations lack busy-state feedback for slow refreshes"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
The e-paper display's `flushSync()` method (`components/cdc_hal/src/EpaperDisplay.cpp:303-311`) performs a blocking refresh that can take 0.5-2 seconds for a full refresh, but there's **no visual feedback** to indicate the display is busy. Users may press keys during this time, expecting immediate response, but the UI appears unresponsive.

**Evidence:**
- `EpaperDisplay::flushSync()` calls `s_epd_display->update()` which blocks for 0.5-2 seconds
- `EpaperDisplay::isBusy()` (`components/cdc_hal/src/EpaperDisplay.cpp:132`) exists but is not used by views
- No loading indicator is shown during flush operations
- E-paper displays are inherently slow; full refreshes take 0.5-2 seconds

## Impact
**User Experience:** During a full refresh (e.g., when navigating between menus), the display freezes for up to 2 seconds. Users pressing keys see no response and may think the device is frozen.

**Technical:** The `isBusy()` method exists but is never checked by the UI layer to prevent user input during refresh.

## Evidence
**File: `components/cdc_hal/src/EpaperDisplay.cpp`**

```cpp
// Line 303-311: Blocking flush with no UI feedback
void EpaperDisplay::flushSync(RefreshMode mode) {
    if (!s_epd_display) return;
    if (mode == RefreshMode::FULL) {
        s_epd_display->update();  // BLOCKS for 0.5-2 seconds!
    } else {
        s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
    }
}

// Line 132: isBusy() exists but is never used
bool EpaperDisplay::isBusy() const override { return s_renderPending; }
```

**File: `components/cdc_ui/src/ViewStack.cpp`**

```cpp
// Line 240-269: Render and flush without checking busy state
void ViewStack::render() {
    IView* view = current();
    if (!view) {
        return;
    }

    // Render current view
    if (viewNeedsRender) {
        view->render(false);
    }

    // Flush display - FULL refresh after view changes, PARTIAL otherwise
    hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
    hal::IDisplay* display = hal::getDisplayInstance();
    if (display) {
        display->flush(mode);  // No check if display is busy
    }
}
```

## Recommended Fix
1. **Add a "refreshing" indicator during slow flushes:**
```cpp
void ViewStack::render() {
    IView* view = current();
    if (!view) {
        return;
    }

    // Render current view
    if (viewNeedsRender) {
        view->render(false);
    }

    // Flush display
    hal::IDisplay* display = hal::getDisplayInstance();
    if (display) {
        hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
        
        // Show "Refreshing..." for full refreshes
        if (mode == hal::RefreshMode::FULL) {
            showToastInfo("Refreshing...");
            ViewStack::instance().render();  // Show toast before blocking
        }
        
        display->flush(mode);  // Blocking call
        
        ViewStack::instance().hideModal();  // Dismiss toast
    }
    needsFullRefresh_ = false;
}
```

2. **Or disable input during refresh:**
```cpp
void ViewStack::dispatchKey(char key) {
    // Check if display is busy
    auto* display = hal::getDisplayInstance();
    if (display && display->isBusy()) {
        return;  // Ignore keys during refresh
    }
    // ... rest of key handling
}
```

## References
- E-paper displays typically take 0.5-2 seconds for full refresh
- `isBusy()` method already exists but is not utilized
