---
title: "[MEDIUM] ListView navigate() wraps to invalid index when itemCount is 0"
severity: MEDIUM
domain: cdc_views/ListView
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `ListView::navigate()` (file: `components/cdc_views/src/ListView.cpp:93-117`), when `itemCount_` is 0, the function returns early at line 94, but the wrap-around logic at lines 104-111 can produce an invalid selection index when `itemCount_` is 1.

Specifically, when `itemCount_ == 1` and the user navigates down from index 0:
- Line 98: `selection_ < itemCount_ - 1` evaluates to `0 < 0` = false
- Line 104: `selection_ = 0` (wraps to top)

This is actually correct behavior. However, when navigating up from index 0 with `itemCount_ == 1`:
- Line 108: `selection_ > 0` evaluates to `0 > 0` = false
- Line 111: `selection_ = itemCount_ - 1` = `0` (correct)

The real edge case is when `itemCount_` changes dynamically while the view is active. If items are removed and `itemCount_` becomes less than `selection_`, the selection can point to an invalid index.

## Impact
- **Out-of-bounds access**: `getSelectedItem()` at line 75-81 can return a pointer to invalid memory if `selection_ >= itemCount_`
- **UI inconsistency**: List may show wrong item as selected
- **Crash potential**: If callback uses userData from invalid selection

## Evidence
File: `components/cdc_views/src/ListView.cpp`

Lines 93-117 (navigate):
```cpp
void ListView::navigate(bool down) {
    if (itemCount_ == 0) return;  // Line 94

    if (down) {
        if (selection_ < itemCount_ - 1) {  // Line 98
            selection_++;
        } else {
            // Wrap-around: bottom to top
            selection_ = 0;
            scrollPos_ = 0;
        }
    } else {
        if (selection_ > 0) {
            selection_--;
        } else {
            // Wrap-around: top to bottom
            selection_ = itemCount_ - 1;  // Line 111
        }
    }

    ensureVisible();
    dirty_ = true;
}
```

Lines 75-81 (getSelectedItem):
```cpp
const ListItem* ListView::getSelectedItem() const {
    if (items_ && selection_ < itemCount_) {  // Line 76
        return &items_[selection_];
    }
    return nullptr;
}
```

Lines 144-149 (onKey - Select):
```cpp
case 'Y': // Select
    if (onSelect_ && items_ && selection_ < itemCount_) {  // Line 146
        onSelect_(selection_, items_[selection_].userData);
    }
    return InputResult::CONSUMED;
```

## Recommended Fix
Add validation in `navigate()` to clamp selection to valid range, and ensure `init()` also validates:

```cpp
void ListView::navigate(bool down) {
    if (itemCount_ == 0) {
        selection_ = 0;
        scrollPos_ = 0;
        return;
    }

    if (down) {
        if (selection_ < itemCount_ - 1) {
            selection_++;
        } else {
            // Wrap-around: bottom to top
            selection_ = 0;
            scrollPos_ = 0;
        }
    } else {
        if (selection_ > 0) {
            selection_--;
        } else {
            // Wrap-around: top to bottom
            selection_ = itemCount_ - 1;
        }
    }

    ensureVisible();
    dirty_ = true;
}

void ListView::init(const char* title, const ListItem* items, uint16_t count) {
    title_ = title;
    items_ = items;
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;

    if (!preservePosition_) {
        selection_ = 0;
        scrollPos_ = 0;
    } else {
        preservePosition_ = false;
        // Clamp selection to valid range
        if (selection_ >= itemCount_) {
            selection_ = itemCount_ > 0 ? itemCount_ - 1 : 0;
        }
        ensureVisible();
    }
    visibleItems_ = VISIBLE_ITEMS;
    dirty_ = true;
}
```

Also add a helper to clamp selection dynamically:
```cpp
void ListView::setSelection(uint16_t index) {
    if (itemCount_ == 0) {
        selection_ = 0;
        return;
    }
    if (index >= itemCount_) {
        index = itemCount_ - 1;
    }
    if (index < selection_) {
        selection_ = index;
        ensureVisible();
        dirty_ = true;
    }
}
```

## References
- CWE-129: Improper validation of array index
- C++ Core Guidelines, [Bounds.1: Use at() to check bounds](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#bounds1-use-at-to-check-bounds)
