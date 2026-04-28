---
title: "[MEDIUM] Missing navigation shortcuts for large lists (page up/down, jump to top/bottom)"
severity: MEDIUM
domain: search-ux
lens: information-architecture
labels:
  - "audit:information-architecture/search-ux"
---

## Summary

The `ListView` component only supports single-item navigation (up/down with keys '2' and '8'). For lists with many items (e.g., password vault with 100+ entries), users must press the key repeatedly to navigate, which is tedious. Missing navigation features include:

1. **Page up/down** - Jump by visible page size (4 items)
2. **Jump to top** - Instantly go to first item
3. **Jump to bottom** - Instantly go to last item
4. **Fast scroll** - Hold key for continuous scrolling

Affects all modules using ListView:
- Password module (up to 162 entries)
- FIDO2 module (up to 27 credentials)
- vCard module (up to 16 peers)

## Impact

**Usability Impact:**
- To navigate from item 1 to item 50 in a password list: 49 key presses
- No way to quickly reach beginning/end of list
- Long lists become frustrating to browse
- Wrap-around navigation (top to bottom) can be confusing for users who want to go "back to start"

**Efficiency:**
- Common navigation patterns require many repeated inputs
- No keyboard shortcuts for power users
- Slower than competing hardware wallet interfaces

**Discovery:**
- Users may not realize they can wrap around (top to bottom)
- No visual indication of navigation speed options

## Evidence

**Current Navigation Implementation** (`components/cdc_views/src/ListView.cpp:94-118`):
```cpp
void ListView::navigate(bool down) {
    if (itemCount_ == 0) return;

    if (down) {
        if (selection_ < itemCount_ - 1) {
            selection_++;  // Single item at a time
        } else {
            // Wrap-around: bottom to top
            selection_ = 0;
            scrollPos_ = 0;
        }
    } else {
        if (selection_ > 0) {
            selection_--;  // Single item at a time
        } else {
            // Wrap-around: top to bottom
            selection_ = itemCount_ - 1;
        }
    }

    ensureVisible();
    dirty_ = true;
}
```

**Key Handling** (`components/cdc_views/src/ListView.cpp:138-167`):
```cpp
InputResult ListView::onKey(char key) {
    switch (key) {
        case '2': // Up
            navigate(false);
            return InputResult::CONSUMED;

        case '8': // Down
            navigate(true);
            return InputResult::CONSUMED;
        // No other navigation keys
    }
}
```

**ListView Configuration** (`components/cdc_views/include/cdc_views/ListView.h:33-37`):
```cpp
static constexpr uint16_t MAX_ITEMS = 512;  // Support large lists (e.g., password vault)
static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 18;
static constexpr uint8_t MIN_VISIBLE_ITEMS = 2;
static constexpr uint8_t MAX_VISIBLE_ITEMS = 8;
```
- Supports up to 512 items
- Only 4-8 visible at a time
- No pagination or fast navigation for large lists

## Recommended Fix

**Add navigation shortcuts** (45-60 min):

1. **Page up/down** (keys '4' and '6'):
   ```cpp
   case '4': // Page up
       selection_ = selection_ >= visibleItems_ ? selection_ - visibleItems_ : 0;
       ensureVisible();
       break;

   case '6': // Page down
       selection_ = std::min(selection_ + visibleItems_, itemCount_ - 1);
       ensureVisible();
       break;
   ```

2. **Jump to top/bottom** (keys '7' and '9'):
   ```cpp
   case '7': // Home (top)
       selection_ = 0;
       scrollPos_ = 0;
       break;

   fast
   case '9': // End (bottom)
       selection_ = itemCount_ - 1;
       ensureVisible();
       break;
   ```

3. **Update footer hint** to show available navigation:
   ```cpp
   const char* getFooterHint() const override {
       if (itemCount_ > visibleItems_) {
           return tr(StringId::HINT_LIST_PAGE);  // "[2/8] Nav  [4/6] Page  [7/9] Top/End"
       }
       return tr(StringId::HINT_LIST);
   }
   ```

**Alternative: Long-press navigation**
- Long-press '2'/'8' for continuous scrolling
- Requires tick-based timing in `onTick()` method

**Files to modify:**
- `components/cdc_views/src/ListView.cpp` - Add page up/down and jump to top/bottom
- `components/cdc_views/include/cdc_views/ListView.h` - Add new key handling
- `components/cdc_ui/include/cdc_ui/I18n.h` - Add new hint strings for navigation

## References

- Navigation patterns for embedded UI: https://www.nngroup.com/articles/keyboard-navigation/
- Hardware wallet navigation: Ledger, Trezor use page-up/down for credential lists
