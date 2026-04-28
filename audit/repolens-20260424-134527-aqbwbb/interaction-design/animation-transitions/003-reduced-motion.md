---
title: "[MEDIUM] No Reduced Motion Support for E-Paper Display"
severity: MEDIUM
domain: animation-transitions
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
The codebase has no support for reducing or disabling motion for users who prefer reduced motion. While this is primarily a web concept (`prefers-reduced-motion`), the same principle applies to embedded UIs: some users may prefer faster, less animated transitions.

**Files affected:**
- All transition-heavy components: `ViewStack.cpp`, `ToastView.cpp`, `MessageBox.cpp`, `SleepManager.cpp`

## Impact
- **Accessibility:** Users sensitive to motion (vestibular disorders) may find E-Paper transitions jarring
- **E-Paper Specific:** E-Paper partial refresh (~350ms) and full refresh (~500-1000ms) can be noticeable
- **Power Users:** Users who want faster feedback have no way to disable transitions

## Evidence
```cpp
// ViewStack.cpp:286 - No option to skip transitions
void ViewStack::showModal(IView* modal) {
    modal_ = modal;
    modal_->onEnter(nullptr);  // No "instant" mode
}

// SleepManager.cpp:112 - Fixed delay, no option to skip
vTaskDelay(pdMS_TO_TICKS(350));  // Always waits 350ms

// ToastView.cpp:46-49 - Fixed auto-dismiss timing
if (durationMs_ > 0 && !expired_) {
    if (nowMs - startMs_ >= durationMs_) {
        expired_ = true;
        ViewStack::instance().hideModal();
    }
}
```

## Recommended Fix
Add a global "reduced motion" preference:

1. **Add to `components/cdc_core/feature_flags.h`:**
   ```cpp
   #define FEATURE_REDUCED_MOTION 1  // Compile-time flag
   ```

2. **Add runtime setting in NVS:**
   ```cpp
   // In settings
   bool getReducedMotion() { ... }
   void setReducedMotion(bool enabled) { ... }
   ```

3. **Modify transition code to respect setting:**
   ```cpp
   void ViewStack::showModal(IView* modal) {
       modal_ = modal;
       modal_->onEnter(nullptr);
       if (!getReducedMotion()) {
           // Apply entrance transition
       }
   }
   ```

4. **For E-Paper:** Reduced motion should use full refresh instead of partial to avoid ghosting

## References
- WCAG 2.1 Success Criterion 2.3.3 (Animation from Interactions)
- MDN: `prefers-reduced-motion` - https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion
