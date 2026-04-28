---
title: "[MEDIUM] ListView scroll position not preserved on view resume (back-navigation from detail view)"
severity: MEDIUM
domain: interaction-design
lens: scroll-behavior
labels:
  - "scroll-restoration"
  - "view-navigation"
---

## Summary

When a user navigates from a `ListView` to a detail view (e.g., editing a password entry) and then returns via back navigation (`N` key), the `ListView::onResume()` method is called (via `ViewStack::pop()` at line 78 in `ViewStack.cpp`). However, `onResume()` only marks the view as dirty (line 133 in `IView.h`) and does **not** preserve or restore the scroll position.

The `ListView` has a `preservePosition()` method (line 124 in `ListView.h`), but it must be called **before** `init()` is invoked. When returning via back navigation, `init()` is not called again - the view simply resumes with its existing state. This works correctly **only if** the list data hasn't changed.

**The actual issue**: When the list data **has** changed (e.g., user added/deleted an entry while the list was on the stack), the scroll position may become invalid because:
1. `itemCount_` may have changed
2. `scrollPos_` may now be out of bounds
3. No automatic clamping happens on resume

**Evidence** (`components/cdc_ui/src/ViewStack.cpp:78-89`):
```cpp
void ViewStack::pop() {
    if (depth_ <= 1) {
        LOG_W(TAG, "Cannot pop root view");
        return;
    }

    // Remove top view
    IView* top = stack_[--depth_];
    if (top) {
        top->onExit();
        LOG_D(TAG, "Popped view '%s' (depth=%d)", top->getName(), depth_);
    }
    stack_[depth_] = nullptr;

    // Resume previous view
    if (depth_ > 0 && stack_[depth_ - 1]) {
        stack_[depth_ - 1]->onResume();  // Just marks dirty, doesn't restore scroll
    }
    ...
}
```

**Evidence** (`components/cdc_ui/include/cdc_ui/IView.h:130-133`):
```cpp
void onResume() override {
    dirty_ = true;  // Only marks as dirty, no scroll restoration
}
```

**Evidence** (`components/cdc_views/src/ListView.cpp:124-131`):
```cpp
void ListView::ensureVisible() {
    if (selection_ >= scrollPos_ + visibleItems_) {
        scrollPos_ = selection_ - visibleItems_ + 1;
    }
    if (selection_ < scrollPos_) {
        scrollPos_ = selection_;
    }
}
```
`ensureVisible()` is only called from `navigate()` and `init()`, not from `onResume()`.

## Impact

- **User Experience**: After returning from a detail view (e.g., editing a password), if the list was rebuilt in the background, the scroll position may be invalid or the list may jump unexpectedly.
- **Inconsistency**: The `preservePosition()` API is designed for explicit save-restore flows, but there's no automatic preservation for simple back-navigation.
- **Edge Cases**: In lists with dynamic content (like the password vault), returning to the list may show an empty area or incorrect scroll position if items were added/removed.

## Evidence

1. `components/cdc_ui/src/ViewStack.cpp:78-89` - `pop()` calls `onResume()` on the previous view
2. `components/cdc_ui/include/cdc_ui/IView.h:130-133` - `ViewBase::onResume()` only sets `dirty_ = true`
3. `components/cdc_views/include/cdc_views/ListView.h:124` - `preservePosition()` must be called before `init()`
4. `components/cdc_views/src/ListView.cpp:124-131` - `ensureVisible()` not called on resume

## Recommended Fix

Add scroll position validation to `ListView::onResume()` to handle cases where list data may have changed:

```cpp
// In ListView.h
class ListView : public ViewBase {
public:
    // ... existing methods ...
    
    void onResume() override;  // Override to add scroll validation
};

// In ListView.cpp
void ListView::onResume() {
    // Call base class to mark dirty
    ViewBase::onResume();
    
    // Validate scroll position in case list size changed
    if (itemCount_ > 0 && visibleItems_ > 0) {
        uint16_t maxScroll = itemCount_ > visibleItems_ ? itemCount_ - visibleItems_ : 0;
        if (scrollPos_ > maxScroll) {
            scrollPos_ = maxScroll;
        }
        if (selection_ >= itemCount_) {
            selection_ = itemCount_ - 1;
        }
        ensureVisible();
    }
}
```

Alternatively, if the module wants full control, it can call `preservePosition()` before pushing the detail view, and the existing `init()` logic will handle restoration when the list is rebuilt.

## References

- `ViewStack::pop()` - `components/cdc_ui/src/ViewStack.cpp:69-89`
- `ViewBase::onResume()` - `components/cdc_ui/include/cdc_ui/IView.h:130-133`
- `ListView::ensureVisible()` - `components/cdc_views/src/ListView.cpp:124-131`
- `ListView::preservePosition()` - `components/cdc_views/include/cdc_views/ListView.h:124`

</content>