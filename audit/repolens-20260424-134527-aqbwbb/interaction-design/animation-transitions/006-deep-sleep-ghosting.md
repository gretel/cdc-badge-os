---
title: "[LOW] Deep sleep transition skips full refresh, may leave ghosting artifacts"
severity: LOW
domain: interaction-design
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary

The deep sleep transition in `LockScreenView::renderDeepSleepScreen()` uses `flushSync(PARTIAL)` to display the transition screen, but partial refresh may leave ghosting artifacts on the E-Paper display. For a significant state change like entering deep sleep, a full refresh would provide a cleaner visual transition.

**Location:** `components/cdc_os_ui/src/views/LockScreenView.cpp:570-578`

## Impact

**Visual Quality:**
- Partial refresh on E-Paper can leave ghosting from previous content
- Deep sleep screen is a significant visual change (clears clock, date, icons)
- User may see residual artifacts from the previous lock screen state

**User Experience:**
- Ghosting can make the deep sleep screen harder to read
- May appear as a "bug" or visual glitch to users
- Deep sleep is a notable state change that deserves a cleaner transition

## Evidence

**Current implementation:**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:570-578
void LockScreenView::renderDeepSleepScreen() {
    deepSleepMode_ = true;

    // Clear clock and status icons for minimal screen
    setClock("");
    setDate("");
    statusIcons_ = StatusIcon::NONE;

    // Render normal lockscreen (with deep sleep footer) and push to display
    render(false);

    auto* display = hal::getDisplayInstance();
    if (display) {
        display->flushSync(hal::RefreshMode::PARTIAL);  // May leave ghosting
    }
}
```

**SleepManager.cpp:118 comment shows awareness of refresh timing:**
```cpp
// Wait for E-Paper partial refresh to complete
vTaskDelay(pdMS_TO_TICKS(350));
```

This comment indicates the team is aware of E-Paper refresh characteristics but still opts for partial refresh in deep sleep transition.

## Recommended Fix

Change the deep sleep transition to use a full refresh for cleaner visuals:

```cpp
void LockScreenView::renderDeepSleepScreen() {
    deepSleepMode_ = true;
    setClock("");
    setDate("");
    statusIcons_ = StatusIcon::NONE;
    render(false);

    auto* display = hal::getDisplayInstance();
    if (display) {
        display->flushSync(hal::RefreshMode::FULL);  // Cleaner transition
    }
}
```

**Alternative (if full refresh is too slow):**
Use partial refresh followed by a brief delay to let the display settle:
```cpp
display->flushSync(hal::RefreshMode::PARTIAL);
vTaskDelay(pdMS_TO_TICKS(200));  // Allow display to settle
```

**Estimated time: 10 minutes**

## References

- E-Paper displays: full refresh eliminates ghosting but takes longer (1-2s)
- Partial refresh: faster (150-400ms) but may leave artifacts
- Deep sleep is a significant state change warranting cleaner visuals
