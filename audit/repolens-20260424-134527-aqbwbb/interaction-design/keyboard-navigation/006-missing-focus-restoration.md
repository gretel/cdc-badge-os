---
title: "[MEDIUM] Missing onEnter/onResume focus restoration pattern for view transitions"
severity: MEDIUM
domain: interaction-design
lens: keyboard-navigation
labels:
  - focus-management
  - view-transitions
  - keyboard-accessibility
---

## Summary

The `IView` interface provides `onEnter()` and `onResume()` lifecycle hooks, but there is no documented pattern or convention for restoring focus state when a view becomes active. This can lead to keyboard users losing their place when navigating back to a previously visited view.

**Evidence locations:**
- `components/cdc_ui/include/cdc_ui/IView.h` - Lifecycle hooks (lines 32-47)
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Navigation methods (lines 25-55)
- `components/cdc_ui/src/ViewStack.cpp` - Lifecycle dispatch (lines 40-58, 68-88)
- `components/cdc_views/src/ListView.cpp` - Partial position preservation (lines 40-58)

The `ViewStack::push()` and `ViewStack::replace()` call `onEnter()`, and `ViewStack::pop()` calls `onResume()` on the previous view, but there is no standard mechanism for views to save/restore their "focus" state (e.g., selected list item, cursor position, scroll offset).

## Impact

**User Experience Impact:**
- When navigating from List A → Detail view → back to List A, the list may reset to the top instead of preserving the previous selection
- Users must re-navigate to their previous position after returning from a child view
- Inconsistent behavior across different view types (some preserve state, some don't)
- Increased cognitive load as users must remember where they were

**Accessibility Impact:**
- Keyboard users rely on predictable focus restoration
- Screen reader users expect focus to return to a logical position after navigating back
- No pattern for "return to previously focused element"

## Evidence

**File: `components/cdc_ui/include/cdc_ui/IView.h` (lines 32-47)**
```cpp
/**
 * Called when view becomes active (pushed or becomes top)
 * @param context Optional context data from parent
 */
virtual void onEnter(void* context = nullptr) = 0;

/**
 * Called when view is being removed from stack
 */
virtual void onExit() override { }

/**
 * Called when view becomes visible again (child popped)
 */
virtual void onResume() override {
    dirty_ = true;
}
```

No documentation about what `onEnter`/`onResume` should do for focus restoration.

**File: `components/cdc_ui/src/ViewStack.cpp` (lines 40-58)**
```cpp
void ViewStack::push(IView* view, void* context) {
    // ...
    stack_[depth_++] = view;
    view->onEnter(context);
    // ListView-to-ListView transitions don't require full refresh
    needsFullRefresh_ = !(isListView(view) && isListView(depth_ > 1 ? stack_[depth_ - 2] : nullptr));
    LOG_D(TAG, "Pushed view '%s' (depth=%d)", view->getName(), depth_);
}
```

`onEnter` is called but no focus state is passed or restored.

**File: `components/cdc_views/src/ListView.cpp` (lines 40-58)**
```cpp
void ListView::init(const char* title, const ListItem* items, uint16_t count) {
    title_ = title;
    items_ = items;
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;

    // Only reset position if not preserving (for back-navigation)
    if (!preservePosition_) {
        selection_ = 0;
        scrollPos_ = 0;
    } else {
        preservePosition_ = false;
        if (selection_ >= itemCount_) {
            selection_ = itemCount_ > 0 ? itemCount_ - 1 : 0;
        }
        ensureVisible();
    }
    // ...
}

void ListView::preservePosition() {
    preservePosition_ = true;
}
```

`ListView` has a `preservePosition()` method, but this is:
1. Not called automatically
2. Not documented as a pattern for other views
3. Only works for list-to-list transitions

## Recommended Fix

### Option 1: Automatic State Preservation for ListView

Modify `ViewStack::pop()` to automatically call `preservePosition()` on ListView instances:

```cpp
void ViewStack::pop() {
    if (depth_ <= 1) {
        LOG_W(TAG, "Cannot pop root view");
        return;
    }

    IView* top = stack_[--depth_];
    if (top) {
        top->onExit();
        LOG_D(TAG, "Popped view '%s' (depth=%d)", top->getName(), depth_);
    }
    stack_[depth_] = nullptr;

    // Resume previous view with automatic state preservation
    if (depth_ > 0 && stack_[depth_ - 1]) {
        // Auto-preserve position for ListView
        if (isListView(stack_[depth_ - 1])) {
            // Cast to ListView and call preservePosition()
            auto* listView = static_cast<ListView*>(stack_[depth_ - 1]);
            listView->preservePosition();
        }
        stack_[depth_ - 1]->onResume();
    }
}
```

### Option 2: Generic Focus State Interface

Add a focus state interface that views can implement:

```cpp
/**
 * FocusState - Interface for views that need focus restoration
 */
struct FocusState {
    uint16_t selectionIndex = 0;
    uint16_t scrollOffset = 0;
    // Add more as needed
};

/**
 * IFocusable - Optional interface for views with focusable elements
 */
class IFocusable {
public:
    virtual ~IFocusable() = default;
    virtual FocusState getFocusState() const = 0;
    virtual void setFocusState(const FocusState& state) = 0;
};

// In ViewStack::push():
void ViewStack::push(IView* view, void* context) {
    // Save focus state of current view if it's focusable
    if (depth_ > 0 && stack_[depth_ - 1]) {
        auto* current = stack_[depth_ - 1];
        if (auto* focusable = dynamic_cast<IFocusable*>(current)) {
            // Store focus state somewhere (NVS, view context, etc.)
        }
    }
    // ...
}
```

### Option 3: Context-Based State Passing

Enhance the `context` parameter in `onEnter()` to carry focus state:

```cpp
struct ViewContext {
    void* data = nullptr;
    uint16_t selectionIndex = 0;  // For list-like views
    uint16_t scrollOffset = 0;
};

void ListView::onEnter(void* context) override {
    if (context) {
        auto* viewContext = static_cast<ViewContext*>(context);
        selection_ = viewContext->selectionIndex;
        scrollPos_ = viewContext->scrollOffset;
        ensureVisible();
    }
    dirty_ = true;
}
```

### Recommended Approach

For minimal code changes, **Option 1** (automatic ListView state preservation) is the quickest win:
1. Modify `ViewStack::pop()` to detect ListView and call `preservePosition()`
2. Add documentation to `IView::onResume()` explaining the pattern
3. Apply the same pattern to other views as needed (DateInputView, TimeInputView, etc.)

## References

- WAI-ARIA Authoring Practices for Focus Management: https://www.w3.org/WAI/ARIA/apg/practices/manage-focus/
- ESP32-S3 CDC Badge navigation patterns
- Legacy implementation reference: `~/GIT/cdc-badge-os-legacy/main/app_input.cpp`

</content>