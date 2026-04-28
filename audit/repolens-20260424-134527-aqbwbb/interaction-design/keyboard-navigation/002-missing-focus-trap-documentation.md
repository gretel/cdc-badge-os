---
title: "[LOW] Focus trap for modal overlays not explicitly documented or enforced"
severity: LOW
domain: interaction-design
lens: keyboard-navigation
labels:
  - focus-trap
  - modal-navigation
  - documentation
---

## Summary

The modal overlay system in `ViewStack` does not explicitly document or enforce a "focus trap" pattern. While modals are shown via `ViewStack::showModal()` and receive key priority, there is no explicit mechanism to prevent the `N` key from bubbling through to underlying views when a modal is active.

**Evidence locations:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Modal methods (lines 106-120)
- `components/cdc_ui/src/ViewStack.cpp` - Modal dispatch logic (lines 159-175, 284-296)
- `components/cdc_views/src/ContextMenuView.cpp` - Modal implementation (lines 260-270)

The current implementation relies on each modal view individually handling the `N` key correctly. If a modal view forgets to handle `N`, the key could potentially be ignored rather than closing the modal.

## Impact

**User Experience Impact:**
- If a modal doesn't handle `N` correctly, users may be "stuck" with no way to dismiss it
- Inconsistent behavior across different modal types could confuse keyboard users
- No visual indication that focus is "trapped" in the modal

**Maintenance Impact:**
- Developers creating new modal views must remember to handle `N` key correctly
- No compile-time or runtime enforcement of the focus trap pattern
- Potential for regressions when refactoring modal views

## Evidence

**File: `components/cdc_ui/include/cdc_ui/ViewStack.h` (lines 106-120)**
```cpp
/**
 * Show a modal overlay (e.g., toast, context menu)
 * @param modal Modal view
 */
void showModal(IView* modal);

/**
 * Hide current modal
 */
void hideModal();

/**
 * Check if modal is active
 */
bool hasModal() const { return modal_ != nullptr; }
```

No documentation about focus trapping or key handling behavior.

**File: `components/cdc_ui/src/ViewStack.cpp` (lines 159-175)**
```cpp
void ViewStack::dispatchKey(char key) {
    // Reset inactivity timer on any key press
    resetInactivityTimer();

    // Modal gets priority
    if (modal_) {
        InputResult result = modal_->onKey(key);
        if (result == InputResult::REQUEST_POP) {
            hideModal();
        }
        return;  // Modal consumes all keys
    }
    // ...
}
```

The modal "consumes all keys" by returning early, but this relies on the modal's `onKey()` implementation to handle keys correctly.

**File: `components/cdc_views/src/ContextMenuView.cpp` (lines 104-124)**
```cpp
InputResult ContextMenuView::onKey(char key) {
    switch (key) {
        case '2': // Up
            navigate(false);
            return InputResult::CONSUMED;

        case '8': // Down
            navigate(true);
            return InputResult::CONSUMED;

        case 'Y': // Select
            select();
            return InputResult::CONSUMED;

        case 'N': // Cancel
            hideContextMenu();
            return InputResult::CONSUMED;  // Note: returns CONSUMED, not REQUEST_POP

        default:
            return InputResult::IGNORED;
    }
}
```

Note that `ContextMenuView` calls `hideContextMenu()` directly instead of returning `REQUEST_POP`, which is a slight inconsistency in the pattern.

## Recommended Fix

1. **Add documentation to `ViewStack::showModal()`** explaining focus trap behavior:
   ```cpp
   /**
    * Show a modal overlay (e.g., toast, context menu)
    *
    * Focus trap: All key events are routed to the modal until dismissed.
    * The modal should handle N key to return InputResult::REQUEST_POP or
    * call hideModal() directly.
    *
    * @param modal Modal view
    */
   void showModal(IView* modal);
   ```

2. **Add a modal base class** with enforced focus trap behavior:
   ```cpp
   class ModalViewBase : public ViewBase {
   public:
       /**
        * Default N-key handling for modals - return REQUEST_POP
        * Override to customize, but always call base class for N key
        */
       virtual InputResult onKey(char key) override {
           if (key == 'N') {
               return InputResult::REQUEST_POP;
           }
           return InputResult::IGNORED;
       }
   };
   ```

3. **Add runtime warning** if a modal doesn't handle common keys:
   ```cpp
   void ViewStack::dispatchKey(char key) {
       if (modal_) {
           InputResult result = modal_->onKey(key);
           if (result == InputResult::IGNORED && key == 'N') {
               LOG_W(TAG, "Modal '%s' ignored N key", modal_->getName());
           }
           // ...
       }
   }
   ```

4. **Standardize modal dismissal pattern** - either always use `REQUEST_POP` or always call `hideModal()` directly, but document which approach to use.

## References

- WAI-ARIA Authoring Practices for Focus Trapping: https://www.w3.org/WAI/ARIA/apg/patterns/dialog-modal/#ex1_desc
- ESP32-S3 CDC Badge navigation patterns
- Legacy implementation reference: `~/GIT/cdc-badge-os-legacy/main/app_input.cpp`
