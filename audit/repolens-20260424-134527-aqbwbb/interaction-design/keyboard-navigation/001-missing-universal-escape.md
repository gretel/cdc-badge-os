---
title: "[MEDIUM] Missing universal Escape/N key handling for modal overlays"
severity: MEDIUM
domain: interaction-design
lens: keyboard-navigation
labels:
  - focus-trap
  - modal-navigation
  - keyboard-accessibility
---

## Summary

The modal overlay system lacks a consistent universal "Escape" key handling pattern. While the `N` key serves as the universal "back/cancel" key in the keypad navigation scheme, several modal components do not properly handle the `N` key for dismissal, creating an inconsistent user experience.

**Evidence locations:**
- `components/cdc_views/include/cdc_views/MessageBox.h` + `MessageBox.cpp` - Only accepts Y/N keys, but behavior is not documented in the class header
- `components/cdc_views/include/cdc_views/ContextMenuView.h` - Properly handles N key (line 117-120)
- `components/cdc_views/include/cdc_views/ConfirmView.h` - Properly handles N key (line 47-52)
- `components/cdc_ui/src/ViewStack.cpp` - Modal dispatch logic (lines 159-175)

The `MessageBox::onKey()` implementation (lines 71-83 in MessageBox.cpp) only accepts Y or N keys:
```cpp
InputResult MessageBox::onKey(char key) {
    // Any key dismisses (Y or N)
    if (key == 'Y' || key == 'N') {
        LOG_D(TAG, "Key '%c' pressed, hiding", key);
        if (onClose_) {
            onClose_();
        }
        hideMessage();
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}
```

While this works, the pattern is not enforced or documented consistently across all modal types.

## Impact

**User Experience Impact:**
- Keyboard users may expect a universal "Escape" key behavior for dismissing modals (a standard pattern in most UI systems)
- Inconsistent modal dismissal behavior can confuse users navigating with the keypad
- The `N` key is documented as "Back" in most views, but this behavior is not guaranteed for all modals

**Maintenance Impact:**
- New modal views may not follow the same pattern without explicit documentation
- No enforcement of the universal "N = cancel/back" convention at the framework level

## Evidence

**File: `components/cdc_views/include/cdc_views/MessageBox.h` (lines 1-28)**
```cpp
/**
 * MessageBox - System feedback overlay
 *
 * Displays a centered message box for user feedback.
 * Can have optional icon and auto-dismiss timeout.
 *
 * Keys:
 *   Y/N = Close (if interactive)
 */
```

**File: `components/cdc_views/src/MessageBox.cpp` (lines 71-83)**
```cpp
InputResult MessageBox::onKey(char key) {
    // Any key dismisses (Y or N)
    if (key == 'Y' || key == 'N') {
        LOG_D(TAG, "Key '%c' pressed, hiding", key);
        if (onClose_) {
            onClose_();
        }
        hideMessage();
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}
```

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
        return;
    }
    // ...
}
```

## Recommended Fix

1. **Document the universal modal dismissal pattern** in the `ViewStack` and `IView` headers:
   - Add clear documentation that `N` key should always dismiss modals
   - Specify that modals should return `InputResult::REQUEST_POP` for N key

2. **Update `MessageBox` documentation** to be consistent with other modals:
   ```cpp
   /**
    * Keys:
    *   Y = Confirm/Close
    *   N = Cancel/Close (universal back key)
    */
   ```

3. **Consider adding a helper method** in `ViewStack` for consistent modal handling:
   ```cpp
   /**
    * Dispatch key to modal with universal N-key back behavior
    */
   void dispatchKeyToModal(char key);
   ```

4. **Add a base class or mixin** for modal views that enforces the N-key behavior:
   ```cpp
   class ModalViewBase : public ViewBase {
   protected:
       virtual void onClose();
       InputResult handleBackKey(char key);  // Returns REQUEST_POP for N
   };
   ```

## References

- WAI-ARIA Authoring Practices for Modal Dialogs: https://www.w3.org/WAI/ARIA/apg/patterns/dialog-modal/
- ESP32-S3 CDC Badge keypad layout: N key is the universal "back" key
- Legacy implementation reference: `~/GIT/cdc-badge-os-legacy/main/app_input.cpp`
