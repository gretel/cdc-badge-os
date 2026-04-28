---
title: "[LOW] ContextMenuView resets scroll position on every open - no position persistence"
severity: LOW
domain: interaction-design
lens: scroll-behavior
labels:
  - "scroll-restoration"
  - "context-menu"
---

## Summary

`ContextMenuView` always resets `scrollPos_` to 0 in its `init()` method (line 41 in `ContextMenuView.cpp`). When a context menu with more than 4 items is opened, the user always sees the first items, even if they had scrolled to see later items in a previous opening of the same menu.

**Evidence** (`components/cdc_views/src/ContextMenuView.cpp:36-42`):
```cpp
void ContextMenuView::init(const char* title, const ContextMenuItem* items, uint8_t count) {
    title_ = title;
    items_ = items;
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;
    selection_ = 0;
    scrollPos_ = 0;  // Always resets to top
    dirty_ = true;
    ...
}
```

## Impact

- **Minor UX friction**: In menus with 5-8 items (the max), users who previously scrolled to see lower items must scroll again each time they open the menu.
- **Inconsistency**: `ListView` has `preservePosition()` support, but `ContextMenuView` does not.

## Evidence

1. `components/cdc_views/src/ContextMenuView.cpp:41` - `scrollPos_ = 0` always set
2. `components/cdc_views/include/cdc_views/ContextMenuView.h:57` - No `preservePosition()` method available
3. `MAX_ITEMS = 8` and `VISIBLE_ITEMS = 4` means scroll is needed for half of all possible menu states

## Recommended Fix

Add optional position preservation to `ContextMenuView`, similar to `ListView`:

```cpp
// In ContextMenuView.h
class ContextMenuView : public ViewBase {
public:
    // ... existing methods ...
    
    void preservePosition() { preservePosition_ = true; }
    
private:
    bool preservePosition_ = false;
    // ... rest of class ...
};

// In ContextMenuView.cpp
void ContextMenuView::init(const char* title, const ContextMenuItem* items, uint8_t count) {
    title_ = title;
    items_ = items;
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;
    
    if (!preservePosition_) {
        selection_ = 0;
        scrollPos_ = 0;
    } else {
        preservePosition_ = false;
        // Ensure selection and scroll are valid for new item count
        if (selection_ >= itemCount_) {
            selection_ = itemCount_ > 0 ? itemCount_ - 1 : 0;
        }
        if (selection_ >= scrollPos_ + VISIBLE_ITEMS) {
            scrollPos_ = selection_ - VISIBLE_ITEMS + 1;
        }
        if (selection_ < scrollPos_) {
            scrollPos_ = selection_;
        }
    }
    
    dirty_ = true;
    ...
}
```

## References

- `ContextMenuView::init()` - `components/cdc_views/src/ContextMenuView.cpp:36-45`
- `ListView::preservePosition()` for reference - `components/cdc_views/include/cdc_views/ListView.h:124`
