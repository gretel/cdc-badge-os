---
title: "[LOW] ListView wrap-around navigation jumps scroll to opposite edge"
severity: LOW
domain: interaction-design
lens: scroll-behavior
labels:
  - "scroll-behavior"
  - "list-navigation"
---

## Summary

When `ListView` navigation wraps around (bottom-to-top or top-to-bottom), the scroll position jumps to the opposite edge instead of maintaining a smoother transition. This can be disorienting for users navigating long lists.

**Evidence** (`components/cdc_views/src/ListView.cpp:97-115`):
```cpp
void ListView::navigate(bool down) {
    if (itemCount_ == 0) return;

    if (down) {
        if (selection_ < itemCount_ - 1) {
            selection_++;
        } else {
            // Wrap-around: bottom to top
            selection_ = 0;
            scrollPos_ = 0;  // Jumps to top
        }
    } else {
        if (selection_ > 0) {
            selection_--;
        } else {
            // Wrap-around: top to bottom
            selection_ = itemCount_ - 1;
            // scrollPos_ NOT reset here - may be inconsistent
        }
    }

    ensureVisible();  // Adjusts scroll after selection change
    dirty_ = true;
}
```

## Impact

- **User confusion**: When at the bottom of a long list and pressing down to wrap to top, the list instantly jumps from showing items 100-104 to items 1-4.
- **Inconsistent behavior**: Bottom-to-top wrap resets scroll, but top-to-bottom wrap relies on `ensureVisible()` which may not produce the expected result.

## Evidence

1. `components/cdc_views/src/ListView.cpp:103` - `scrollPos_ = 0` on bottom-to-top wrap
2. `components/cdc_views/src/ListView.cpp:110-111` - Top-to-bottom wrap does NOT reset scrollPos directly
3. `components/cdc_views/src/ListView.cpp:124-131` - `ensureVisible()` adjusts scroll but may not match user expectation on wrap

## Recommended Fix

Consider adding visual feedback or a "pulse" animation for wrap-around, or make scroll behavior consistent:

```cpp
void ListView::navigate(bool down) {
    if (itemCount_ == 0) return;

    if (down) {
        if (selection_ < itemCount_ - 1) {
            selection_++;
        } else {
            // Wrap-around: bottom to top
            selection_ = 0;
            // Keep scroll position briefly, then adjust
            // This creates a smoother visual transition
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
```

Alternatively, for a more significant improvement, consider adding a "bounce" visual effect where the list briefly shows an empty space before wrapping.

## References

- `ListView::navigate()` - `components/cdc_views/src/ListView.cpp:97-117`
- `ListView::ensureVisible()` - `components/cdc_views/src/ListView.cpp:124-131`
