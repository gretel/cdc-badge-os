---
title: "[MEDIUM] No universal Escape key for canceling actions across all views"
severity: MEDIUM
domain: interaction-design
lens: keyboard-navigation
labels:
  - keyboard-shortcuts
  - modal-navigation
  - keyboard-accessibility
---

## Summary

The ESP32-S3 CDC Badge uses a 12-button keypad where the `N` key serves as the universal "Cancel/Back" key. However, there is no consistent documentation or enforcement that all views should handle the `N` key for cancellation. Some views handle it correctly, while others may not provide an obvious way to cancel long-running operations.

**Evidence locations:**
- `components/cdc_hal/include/cdc_hal/IKeypad.h` - Key definitions (lines 16-21)
- `components/cdc_views/include/cdc_views/*.h` - Various view headers
- `components/cdc_os_ui/src/AppUi.cpp` - Main input loop (lines 756-762)

The keypad layout is:
```
  [1] [2] [3]
  [4] [5] [6]
  [7] [8] [9]
  [N] [0] [Y]   (N=Cancel, Y=OK)
```

While this is documented in `IKeypad.h`, the convention that `N` should be the universal "Escape" key is not enforced or consistently documented across all views.

## Impact

**User Experience Impact:**
- Users may get "stuck" in views that don't handle the `N` key for cancellation
- Inconsistent behavior between views creates a learning curve
- No visual indication of what key to press for cancellation in all views

**Accessibility Impact:**
- Keyboard-only users rely on predictable cancellation patterns
- The `N` key is the only "back" key on the hardware, making it critical
- No fallback when a view forgets to handle `N`

## Evidence

**File: `components/cdc_hal/include/cdc_hal/IKeypad.h` (lines 8-21)**
```cpp
/**
 * Key codes for the 12-button keypad
 * Physical layout:
 *   [1] [2] [3]
 *   [4] [5] [6]
 *   [7] [8] [9]
 *   [N] [0] [Y]   (N=Cancel, Y=OK)
 */
enum class Key : char {
    KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
    KEY_4 = '4', KEY_5 = '5', KEY_6 = '6',
    KEY_7 = '7', KEY_8 = '8', KEY_9 = '9',
    KEY_NO = 'N', KEY_0 = '0', KEY_YES = 'Y',
    KEY_NONE = 0
};
```

The `N=Cancel` convention is documented here, but not enforced elsewhere.

**File: `components/cdc_os_ui/src/AppUi.cpp` (lines 756-762)**
```cpp
hal::Key key = s_deps.keypad->getNextKey();
if (key != hal::Key::KEY_NONE) {
    char keyChar = static_cast<char>(key);
    ViewStack::instance().dispatchKey(keyChar);
    SleepManager::instance().resetTimer(nowMs);
}
```

Keys are dispatched but no validation that `N` is handled.

**File: `components/cdc_views/src/ListView.cpp` (lines 147-151)**
```cpp
case 'N': // Back
    return InputResult::REQUEST_POP;
```

ListView handles `N` correctly.

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

MessageBox handles both `Y` and `N`, but the comment says "Any key" which is misleading.

## Recommended Fix

### 1. Document the Universal N-Key Convention

Add clear documentation to the `IView` interface:

```cpp
/**
 * Handle key press
 * @param key Key character ('0'-'9', 'Y', 'N', etc.)
 * @return Input result
 *
 * Universal key conventions:
 * - N = Back/Cancel (should be handled by all views)
 * - Y = Confirm/OK (when applicable)
 * - 2/8 = Up/Down navigation (for list-like views)
 */
virtual InputResult onKey(char key) = 0;
```

### 2. Add a Base Class with Default N-Key Handling

```cpp
class ModalViewBase : public ViewBase {
public:
    /**
     * Default N-key handling for modals - return REQUEST_POP
     * Override to customize, but always handle N key
     */
    virtual InputResult onKey(char key) override {
        if (key == 'N') {
            return InputResult::REQUEST_POP;
        }
        return InputResult::IGNORED;
    }
};
```

### 3. Add Runtime Warning for Unhandled N Keys

```cpp
void ViewStack::dispatchKey(char key) {
    if (modal_) {
        InputResult result = modal_->onKey(key);
        if (result == InputResult::IGNORED && key == 'N') {
            LOG_W(TAG, "Modal '%s' ignored N key - should handle cancel", modal_->getName());
        }
        if (result == InputResult::REQUEST_POP) {
            hideModal();
        }
        return;
    }
    // ...
}
```

### 4. Update All View Headers

Ensure all view headers document the keys they handle:

```cpp
/**
 * Keys:
 *   Y = Confirm
 *   N = Cancel/Back (universal)
 *   2/8 = Navigate up/down
 */
class ListView : public ViewBase { ... };
```

## References

- WAI-ARIA Authoring Practices for Keyboard Navigation: https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/
- ESP32-S3 CDC Badge 12-button keypad layout
- Legacy implementation reference: `~/GIT/cdc-badge-os-legacy/main/app_input.cpp`
