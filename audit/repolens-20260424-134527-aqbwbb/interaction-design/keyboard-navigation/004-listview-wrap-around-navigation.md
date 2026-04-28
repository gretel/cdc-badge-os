---
title: "[MEDIUM] ListView wrap-around navigation may confuse users"
severity: MEDIUM
domain: interaction-design
lens: keyboard-navigation
labels:
  - list-navigation
  - keyboard-accessibility
  - user-experience
---

## Summary

The `ListView` component implements wrap-around navigation (bottom-to-top and top-to-bottom), which may be unexpected for users familiar with standard list behavior. This creates an "infinite scroll" effect that can disorient keyboard users who expect clear boundaries.

**Evidence locations:**
- `components/cdc_views/src/ListView.cpp` - Navigation implementation (lines 91-117)
- `components/cdc_views/include/cdc_views/ListView.h` - No documentation of wrap-around behavior

The wrap-around is implemented but not documented in the class header or shown in the UI (no visual indicator when wrapping occurs).

## Impact

**User Experience Impact:**
- Users may "lose their place" when navigating a long list
- No visual indication that wrap-around occurred
- Users expecting boundaries (top/bottom of list) may be confused
- Can make it harder to find specific items in large lists

**Accessibility Impact:**
- Screen reader users typically expect clear list boundaries
- Users with cognitive disabilities may find wrap-around disorienting
- Power users may prefer wrap-around, but it should be opt-in or documented

## Evidence

**File: `components/cdc_views/src/ListView.cpp` (lines 91-117)**
```cpp
void ListView::navigate(bool down) {
    if (itemCount_ == 0) return;

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

    LOG_D(TAG, "navigate: sel=%d, scroll=%d", selection_, scrollPos_);
}
```

The wrap-around behavior is clear from the code, but:
1. Not documented in the header
2. No visual feedback when wrapping occurs
3. No way to disable it

**File: `components/cdc_views/include/cdc_views/ListView.h` (lines 26-31)**
```cpp
/**
 * ListView - Highly scrollable selection menu
 *
 * Reusable component for any list-based UI.
 * Dynamically calculates visible items based on display size.
 *
 * Keys:
 *   2 = Up
 *   8 = Down
 *   Y = Select (triggers callback)
 *   N = Back (REQUEST_POP)
 */
```

No mention of wrap-around behavior in the documentation.

**Footer hint** (line 266-271 in ListView.cpp):
```cpp
// Footer with position counter
char positionStr[16];
const char* prefix = nullptr;
if (itemCount_ > 0) {
    snprintf(positionStr, sizeof(positionStr), "%u/%u  ", selection_ + 1, itemCount_);
    prefix = positionStr;
}
```

Shows position (e.g., "1/10"), but doesn't indicate boundaries or wrap-around state.

## Recommended Fix

1. **Document wrap-around behavior** in the class header:
   ```cpp
   /**
    * Keys:
    *   2 = Up (wraps to bottom at top)
    *   8 = Down (wraps to top at bottom)
    *   Y = Select (triggers callback)
    *   N = Back (REQUEST_POP)
    *
    * Note: Navigation wraps around at list boundaries.
    */
   ```

2. **Add optional non-wrapping mode**:
   ```cpp
   class ListView : public ViewBase {
   public:
       void setWrapAround(bool wrap);  // Default: true for backward compat
       bool getWrapAround() const;
   private:
       bool wrapAround_ = true;
   };
   ```

3. **Add visual feedback for wrap-around**:
   ```cpp
   void ListView::navigate(bool down) {
       bool wrapped = false;
       if (down) {
           if (selection_ < itemCount_ - 1) {
               selection_++;
           } else {
               selection_ = 0;
               scrollPos_ = 0;
               wrapped = true;
           }
       } else {
           if (selection_ > 0) {
               selection_--;
           } else {
               selection_ = itemCount_ - 1;
               wrapped = true;
           }
       }
       if (wrapped) {
           // Optional: flash indicator or show toast
           // showWrapIndicator();
       }
       ensureVisible();
       dirty_ = true;
   }
   ```

4. **Improve footer to indicate boundaries**:
   ```cpp
   // Show arrows for available navigation
   char hint[32];
   snprintf(hint, sizeof(hint), "%u/%u  %s%s",
            selection_ + 1, itemCount_,
            selection_ > 0 ? "2" : "↑",  // Up arrow or "2"
            selection_ < itemCount_ - 1 ? "8" : "↓");  // Down arrow or "8"
   ```

5. **Consider adding sound haptic feedback** (if hardware supports) when wrapping occurs.

## References

- WAI-ARIA Authoring Practices for Listboxes: https://www.w3.org/WAI/ARIA/apg/patterns/listbox/
- Common list navigation patterns in embedded UIs
- ESP32-S3 CDC Badge 12-button keypad layout
