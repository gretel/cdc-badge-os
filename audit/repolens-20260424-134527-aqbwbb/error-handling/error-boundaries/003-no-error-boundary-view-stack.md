---
title: "[MEDIUM] No error boundary in ViewStack dispatch methods"
severity: MEDIUM
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "ui-framework"
---

## Summary
The `ViewStack` class in `components/cdc_ui/src/ViewStack.cpp` dispatches input and tick events to views without error isolation. A single view throwing during `onKey()`, `onLongPress()`, or `onTick()` will crash the entire UI system.

**Evidence:**
- `components/cdc_ui/src/ViewStack.cpp:159-172` (dispatchKey):
```cpp
void ViewStack::dispatchKey(char key) {
    ResetInactivityTimer on any key press

    // Modal gets priority
    if (modal_) {
        InputResult result = modal_->onKey(key);  // No error boundary!
        if (result == InputResult::REQUEST_POP) {
            hideModal();
        }
        return;
    }

    // Dispatch to current view
    IView* view = current();
    if (view) {
        InputResult result = view->onKey(key);  // No error boundary!
        if (result == InputResult::REQUEST_POP) {
            pop();
        }
    }
}
```

- `components/cdc_ui/src/ViewStack.cpp:223-231` (dispatchTick):
```cpp
void ViewStack::dispatchTick(uint32_t nowMs) {
    // Tick both modal and current view
    if (modal_) {
        modal_->onTick(nowMs);  // No error boundary!
    }
    IView* view = current();
    if (view) {
        view->onTick(nowMs);  // No error boundary!
    }
}
```

## Impact
- **UI crash**: Single buggy view can freeze the entire display
- **No graceful degradation**: Rest of UI remains functional but inaccessible
- **User lockout**: May require device reboot to recover

## Recommended Fix
Add error boundaries around view method calls in `ViewStack`:

```cpp
void ViewStack::dispatchKey(char key) {
    resetInactivityTimer();

    if (modal_) {
        try {
            InputResult result = modal_->onKey(key);
            if (result == InputResult::REQUEST_POP) {
                hideModal();
            }
        } catch (...) {
            LOG_E(TAG, "Modal view key exception");
            hideModal();  // Recover by hiding modal
        }
        return;
    }

    IView* view = current();
    if (view) {
        try {
            InputResult result = view->onKey(key);
            if (result == InputResult::REQUEST_POP) {
                pop();
            }
        } catch (...) {
            LOG_E(TAG, "Current view key exception");
        }
    }
}

void ViewStack::dispatchTick(uint32_t nowMs) {
    if (modal_) {
        try {
            modal_->onTick(nowMs);
        } catch (...) {
            LOG_E(TAG, "Modal view tick exception");
        }
    }
    IView* view = current();
    if (view) {
        try {
            view->onTick(nowMs);
        } catch (...) {
            LOG_E(TAG, "Current view tick exception");
        }
    }
}
```

## References
- [UI Error Boundaries](https://opencode.ai/guides/error-handling/ui-error-boundaries/)
- [View Pattern Best Practices](https://en.cppreference.com/w/cpp/language/virtual)
