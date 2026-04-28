---
title: "[MEDIUM] Inactivity Timeout Doesn't Dismiss Modal First"
severity: MEDIUM
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The inactivity timeout handler (`onInactivityTimeout`) pops the view stack to return to the lock screen, but it doesn't first hide any active modal. This can leave a modal overlay visible after the timeout, creating a confusing state.

**Files affected:**
- `components/cdc_os_ui/src/AppUi.cpp:304-311`
- `components/cdc_ui/include/cdc_ui/ViewStack.h:114`

## Impact
If a modal (toast, confirmation dialog, context menu) is showing when the inactivity timeout expires:
1. The stack is popped to depth 1 (lock screen)
2. The modal remains visible on top of the lock screen
3. User sees lock screen + modal overlay, which is unexpected
4. May require manual interaction to dismiss the modal

## Evidence
From `AppUi.cpp:304-311`:
```cpp
static void onInactivityTimeout() {
    // Lock screen
    while (ViewStack::instance().depth() > 1) {
        ViewStack::instance().pop();
    }
    s_ignoreKeyUntilRelease = true;
    clearKeypadBuffer();
    // Note: No hideModal() call!
}
```

From `ViewStack.h:114`:
```cpp
void hideModal();  // Available but not called
```

## Recommended Fix
Add `hideModal()` call at the start of `onInactivityTimeout()`:

```cpp
static void onInactivityTimeout() {
    // First dismiss any modal overlay
    if (ViewStack::instance().hasModal()) {
        ViewStack::instance().hideModal();
    }
    // Then return to lock screen
    while (ViewStack::instance().depth() > 1) {
        ViewStack::instance().pop();
    }
    s_ignoreKeyUntilRelease = true;
    clearKeypadBuffer();
}
```

This ensures a clean transition to the lock screen regardless of what modal was showing.

## References
- Modal handling in ViewStack: `components/cdc_ui/src/ViewStack.cpp:286-313`
- ToastView, ConfirmView, ContextMenuView all use `showModal()`
