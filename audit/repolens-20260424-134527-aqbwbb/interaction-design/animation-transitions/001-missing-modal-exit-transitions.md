---
title: "[HIGH] Missing Exit Transitions for Modal Overlays"
severity: HIGH
domain: interaction-design
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
Modal overlays (ToastView, MessageBox, ContextMenuView) appear instantly without any entrance transition and disappear instantly without any exit transition. This is particularly noticeable on E-Paper displays where abrupt changes can feel jarring.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp:175` - `showToastInternal()`
- `components/cdc_views/src/MessageBox.cpp:209` - `showMessage()`
- `components/cdc_views/src/ContextMenuView.cpp:256` - `showContextMenu()`
- `components/cdc_ui/src/ViewStack.cpp:286` - `showModal()`

## Impact
- **User Experience:** Instant pop-in/pop-out transitions feel abrupt and less polished
- **Visual Continuity:** No visual feedback for when modals appear/disappear
- **E-Paper Specific:** E-Paper displays already have slower refresh; abrupt changes are more noticeable than on LCD/OLED

## Evidence
```cpp
// ToastView.cpp:175 - No animation, immediate push
static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible = true) {
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    ViewStack::instance().render();  // Immediate render for toast
}

// ViewStack.cpp:286 - Modal appears instantly
void ViewStack::showModal(IView* modal) {
    if (modal_) {
        modal_->onExit();
    }
    modal_ = modal;
    if (modal_) {
        modal_->onEnter(nullptr);
        LOG_D(TAG, "Showing modal '%s'", modal_->getName());
    }
}

// MessageBox.cpp:218 - Modal disappears instantly
void hideMessage() {
    ViewStack::instance().hideModal();  // No fade-out, instant removal
}
```

## Recommended Fix
Implement a simple fade-in/fade-out transition for modal overlays:

1. **Add transition state to modal views:**
   ```cpp
   enum class ModalState { ENTRANCE, ACTIVE, EXIT };
   ```

2. **Implement multi-step render for transitions:**
   ```cpp
   void ToastView::render(bool partial) {
       // Draw with opacity based on state
       // E-Paper: Use dithering pattern for "fade" effect
   }
   ```

3. **Add transition timing:**
   ```cpp
   // In ViewStack
   void showModal(IView* modal, uint16_t transitionMs = 150);
   void hideModal(uint16_t transitionMs = 150);
   ```

4. **For E-Paper specifically:** Use a dithering pattern or checkerboard fade since true alpha blending is not available.

## References
- WCAG 2.1 Success Criterion 2.2.3 (No Timing): Animations should be dismissible
- E-Paper display characteristics: Partial refresh ~350ms, Full refresh ~500-1000ms
- Related issue: [To be created - Missing timing tokens]
