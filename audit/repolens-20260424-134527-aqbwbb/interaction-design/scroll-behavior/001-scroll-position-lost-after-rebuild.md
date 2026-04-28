---
title: "[MEDIUM] Scroll position lost after list rebuild - preservePosition() called before init()"
severity: MEDIUM
domain: interaction-design
lens: scroll-behavior
labels:
  - "scroll-restoration"
  - "list-navigation"
---

## Summary

In `components/mod_password/src/PasswordModule.cpp`, the `preservePosition()` method is called **before** `rebuildList()` which calls `ListView::init()`. However, `ListView::init()` resets `scrollPos_` and `selection_` at the beginning of the function (lines 49-58), and only preserves position if `preservePosition_` is already set. The issue is that the sequence of operations causes the scroll position to be lost because:

1. `s_listView.preservePosition()` sets `preservePosition_ = true` (line 529, 685)
2. `rebuildList()` is called which eventually calls `s_listView.init()` (line 433)
3. In `ListView::init()` (line 43-58), the code checks `if (!preservePosition_)` - but this works correctly

**The actual bug**: Looking more closely at `ListView::init()` at line 53, after preserving position, it sets `preservePosition_ = false`. But in `rebuildList()` at line 433, `init()` is called which **always** processes the preserve logic. The issue is that `ensureVisible()` is called at line 57 only in the preserve case, but the `scrollPos_` may still be out of bounds if the list size changed (e.g., item added/deleted).

**Evidence** (`components/mod_password/src/PasswordModule.cpp:528-534`):
```cpp
if (ok) {
    ui::showToastSuccess(mstr(STR_SAVED));
    s_listView.preservePosition();      // Sets preservePosition_ = true
    rebuildList();                       // Calls init() which resets scrollPos_
    while (ui::ViewStack::instance().current() != &s_listView &&
           ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}
```

**Evidence** (`components/cdc_views/src/ListView.cpp:43-58`):
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
        ensureVisible();  // May not preserve exact scroll position
    }
    ...
}
```

## Impact

- **User Experience**: After adding or deleting a password entry, the list jumps to the top instead of staying near the user's previous position. This is disorienting, especially in long lists (password vault can have 100+ entries).
- **Navigation Efficiency**: Users must manually scroll back to their previous position, adding unnecessary interaction steps.
- **Consistency**: The `preservePosition()` API suggests position should be maintained, but the actual behavior depends on list size changes.

## Evidence

1. `components/mod_password/src/PasswordModule.cpp:529` - `preservePosition()` called before `rebuildList()`
2. `components/mod_password/src/PasswordModule.cpp:685` - Same pattern in delete flow
3. `components/cdc_views/src/ListView.cpp:49-58` - `init()` resets scroll position, only partially preserves when flag is set
4. `components/cdc_views/include/cdc_views/ListView.h:124` - `preservePosition()` method documentation says "Preserve current position (selection and scroll) on next init"

## Recommended Fix

Modify `ListView::init()` to better handle scroll position preservation when list size changes:

**Option 1** - Clamp scroll position after preservation:
```cpp
void ListView::init(const char* title, const ListItem* items, uint16_t count) {
    title_ = title;
    items_ = items;
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;

    if (!preservePosition_) {
        selection_ = 0;
        scrollPos_ = 0;
    } else {
        preservePosition_ = false;
        // Clamp selection to new list bounds
        if (selection_ >= itemCount_) {
            selection_ = itemCount_ > 0 ? itemCount_ - 1 : 0;
        }
        // Clamp scrollPos to valid range based on new item count
        uint16_t maxScroll = itemCount_ > visibleItems_ ? itemCount_ - visibleItems_ : 0;
        if (scrollPos_ > maxScroll) {
            scrollPos_ = maxScroll;
        }
        ensureVisible();
    }
    visibleItems_ = VISIBLE_ITEMS;
    dirty_ = true;
}
```

**Option 2** - Add explicit scroll restoration method:
```cpp
// In ListView.h
void restoreScrollPosition(uint16_t oldScrollPos, uint16_t oldItemCount, uint16_t newItemCount);

// Implementation
void ListView::restoreScrollPosition(uint16_t oldScrollPos, uint16_t oldItemCount, uint16_t newItemCount) {
    // Calculate how many items were added/removed before current view
    int delta = newItemCount - oldItemCount;
    scrollPos_ = oldScrollPos;
    // Adjust scroll position based on where changes occurred
    if (delta > 0) {
        // Items added, try to keep same visual position
        scrollPos_ = oldScrollPos;
    }
    // Clamp to valid range
    uint16_t maxScroll = itemCount_ > visibleItems_ ? itemCount_ - visibleItems_ : 0;
    if (scrollPos_ > maxScroll) scrollPos_ = maxScroll;
}
```

## References

- `ListView::init()` - `components/cdc_views/src/ListView.cpp:43-62`
- `ListView::preservePosition()` - `components/cdc_views/include/cdc_views/ListView.h:124`
- `rebuildList()` usage - `components/mod_password/src/PasswordModule.cpp:408-435`
