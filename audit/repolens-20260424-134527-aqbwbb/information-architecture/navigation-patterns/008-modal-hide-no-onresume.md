---
title: "[MEDIUM] Modal hide doesn't trigger onResume() on underlying view"
severity: MEDIUM
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
When a modal overlay is hidden via `ViewStack::hideModal()`, the underlying view's `onResume()` lifecycle hook is not called. This is inconsistent with the `pop()` function which properly calls `onResume()` on the resumed view. Views that rely on `onResume()` to refresh data (e.g., updating list contents, refreshing status) will not be notified when a modal is dismissed.

**Evidence locations:**
- `components/cdc_ui/src/ViewStack.cpp:287-312` - `hideModal()` implementation
- `components/cdc_ui/src/ViewStack.cpp:68-89` - `pop()` implementation (calls `onResume()`)
- `components/cdc_ui/include/cdc_ui/IView.h:43-48` - `onResume()` lifecycle method definition

## Impact
**Inconsistent lifecycle:** Modal dismissal and view popping should have similar lifecycle behavior, but they don't.
**Stale data:** Views that refresh data in `onResume()` (e.g., after a modal form is completed) won't update.
**Confusion:** Developers may expect `onResume()` to be called in both cases but it's only called on `pop()`.

## Evidence
```cpp
// In ViewStack.cpp - pop() calls onResume()
void ViewStack::pop() {
    // ...
    IView* top = stack_[--depth_];
    if (top) {
        top->onExit();
    }
    // Resume previous view
    if (depth_ > 0 && stack_[depth_ - 1]) {
        stack_[depth_ - 1]->onResume();  // Called here
    }
}

// In ViewStack.cpp - hideModal() does NOT call onResume()
void ViewStack::hideModal() {
    if (modal_) {
        modal_->onExit();
        modal_ = nullptr;

        // Mark current view as dirty to redraw
        IView* view = current();
        if (view) {
            view->markDirty();  // Only marks dirty, doesn't call onResume()
        }
    }
}
```

## Recommended Fix
Call `onResume()` on the underlying view when hiding a modal:

```cpp
void ViewStack::hideModal() {
    if (modal_) {
        modal_->onExit();
        modal_ = nullptr;

        // Call onResume() for consistency with pop()
        IView* view = current();
        if (view) {
            view->onResume();  // Add this line
        }
    }
}
```

## References
- `components/cdc_ui/include/cdc_ui/IView.h` - View lifecycle interface
- `components/cdc_ui/src/ViewStack.cpp` - ViewStack implementation
