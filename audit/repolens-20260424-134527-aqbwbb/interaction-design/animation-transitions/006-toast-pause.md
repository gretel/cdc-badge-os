---
title: "[LOW] Toast Auto-Dismiss Doesn't Pause on Focus Change"
severity: LOW
domain: animation-transitions
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
Toast messages auto-dismiss based on elapsed time without pausing when the user navigates away and returns. This can cause toasts to disappear before the user has a chance to see them if they're interacting with other views.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp:46-51` - `onTick()` implementation
- `components/cdc_views/src/ToastView.cpp:175` - `showToastInternal()`

## Impact
- **User Experience:** Toast may disappear while user is looking at another view
- **Information Loss:** Important feedback (success/error) may be missed
- **Accessibility:** Users who need more time to read may miss the message

## Evidence
```cpp
// ToastView.cpp:46-51
void ToastView::onTick(uint32_t nowMs) {
    if (durationMs_ > 0 && !expired_) {
        if (nowMs - startMs_ >= durationMs_) {
            expired_ = true;
            ViewStack::instance().hideModal();
        }
    }
}

// ToastView.cpp:175
static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible = true) {
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    ViewStack::instance().render();  // Immediate render
}
```

The toast timer continues running even if:
1. User navigates to another view
2. User is interacting with a modal
3. Device is in light sleep

## Recommended Fix
Pause toast timer when modal is not visible:

1. **Track visibility state:**
   ```cpp
   void ToastView::onTick(uint32_t nowMs) {
       if (durationMs_ > 0 && !expired_ && isVisible()) {
           if (nowMs - startMs_ >= durationMs_) {
               expired_ = true;
               ViewStack::instance().hideModal();
           }
       }
   }
   ```

2. **Pause on view change:**
   ```cpp
   void ToastView::onExit() {
       // Pause timer
       pausedMs_ = esp_timer_get_time() / 1000;
   }
   
   void ToastView::onResume() {
       // Resume timer
       if (pausedMs_ > 0) {
           startMs_ += esp_timer_get_time() / 1000 - pausedMs_;
       }
   }
   ```

3. **Alternative:** Extend toast duration by the pause time

## References
- Material Design Toast guidelines: https://m3.material.io/components/toasts/guidelines
- WCAG 2.1 Success Criterion 1.4.4 (Resize text): Content should be readable
