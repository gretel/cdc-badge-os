---
title: "[LOW] Inactivity Timer Not Reset on View Change"
severity: LOW
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The inactivity timer is only reset on key presses (`dispatchKey`), but not when views change programmatically (e.g., wizard steps, auto-navigation). This can cause the timeout to trigger unexpectedly during multi-step wizards if the user pauses between steps.

**Files affected:**
- `components/cdc_ui/src/ViewStack.cpp:334-340` (resetInactivityTimer)
- `components/cdc_os_ui/src/AppUi.cpp:57` (5-minute timeout)

## Impact
User experience issues:
1. User starts a TOTP wizard (5 steps: name, secret, issuer, digits, algorithm, period)
2. User pauses between steps to think or read
3. If pause exceeds 5 minutes, timeout triggers mid-wizard
4. User loses wizard state and has to start over

## Evidence
From `ViewStack.cpp:334-340`:
```cpp
void ViewStack::resetInactivityTimer() {
    lastActivityMs_ = 0;  // Will be updated on next checkInactivity
}
```

From `ViewStack.cpp:159-162`:
```cpp
void ViewStack::dispatchKey(char key) {
    // Reset inactivity timer on any key press
    resetInactivityTimer();
    // ...
}
```

From `AppUi.cpp:57`:
```cpp
static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
```

**Problem:** The timer resets on key press, but if a wizard step takes user >5 seconds to complete (e.g., typing a long secret), and then they pause to think, the timeout can fire.

Also, `resetInactivityTimer()` sets `lastActivityMs_ = 0`, which means the next `checkInactivity()` call will just initialize it (see `ViewStack.cpp:352-355`). This is correct but relies on `checkInactivity()` being called frequently enough.

## Recommended Fix
Reset the timer when pushing new views (for user-initiated navigation):

**Option 1: Reset in push/replace methods**
```cpp
void ViewStack::push(IView* view, void* context) {
    // ... existing code ...
    stack_[depth_++] = view;
    view->onEnter(context);
    // Reset timer on view change
    lastActivityMs_ = 0;  // Reset for new view
}
```

**Option 2: Add explicit method for programmatic navigation**
```cpp
/**
 * \brief Reset inactivity timer (for programmatic navigation)
 * Call after push/replace if navigation was not user-initiated
 */
void resetInactivityTimerForNavigation() {
    lastActivityMs_ = esp_timer_get_time() / 1000;  // Set actual time
}
```

**Option 3: Configure per-view timeout**
Some views (like wizards) could have longer timeouts:
```cpp
class WizardView {
    void onEnter(void* context) {
        // Set longer timeout for wizard
        ViewStack::instance().setInactivityTimeout(
            onInactivityTimeout, 
            10 * 60 * 1000  // 10 minutes for wizard
        );
    }
    
    void onResume() {
        // Restore original timeout
        ViewStack::instance().setInactivityTimeout(
            onInactivityTimeout, 
            5 * 60 * 1000  // 5 minutes normally
        );
    }
};
```

**Option 4: Use actual timestamp in reset**
```cpp
void resetInactivityTimer() {
    lastActivityMs_ = esp_timer_get_time() / 1000;  // Set actual time, not 0
}
```

This makes the timer more predictable - it resets to "now" instead of relying on the next check to initialize.

## References
- Inactivity timeout setup: `components/cdc_os_ui/src/AppUi.cpp:685`
- Timer check logic: `components/cdc_ui/src/ViewStack.cpp:342-365`
