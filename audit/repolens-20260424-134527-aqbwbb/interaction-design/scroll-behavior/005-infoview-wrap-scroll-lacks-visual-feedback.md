---
title: "[LOW] InfoView wrap-around scrolling lacks visual feedback"
severity: LOW
domain: interaction-design
lens: scroll-behavior
labels:
  - "scroll-behavior"
  - "info-view"
---

## Summary

`InfoView` implements wrap-around scrolling (top-to-bottom and bottom-to-top), but provides no visual indication that wrapping occurred. Users may not realize they've reached the edge and scrolled to the opposite end.

**Evidence** (`components/cdc_views/src/InfoView.cpp:81-99`):
```cpp
void InfoView::scroll(bool down) {
    if (totalLines_ <= VISIBLE_LINES) return;

    if (down) {
        if (scrollLine_ < totalLines_ - VISIBLE_LINES) {
            scrollLine_++;
        } else {
            // Wrap to top
            scrollLine_ = 0;  // No visual indication of wrap
        }
    } else {
        if (scrollLine_ > 0) {
            scrollLine_--;
        } else {
            // Wrap to bottom
            scrollLine_ = totalLines_ - VISIBLE_LINES;  // No visual indication
        }
    }

    dirty_ = true;
}
```

## Impact

- **Disorientation**: Users scrolling at the edge may not realize wrapping occurred, especially if text content is similar at top and bottom.
- **Unclear state**: No visual cue indicates whether user is at the "natural" start/end or has wrapped.

## Evidence

1. `components/cdc_views/src/InfoView.cpp:88-90` - Wrap to top with no visual feedback
2. `components/cdc_views/src/InfoView.cpp:95-97` - Wrap to bottom with no visual feedback
3. Scroll indicators (`components/cdc_views/src/RenderHelpers.cpp:102-114`) only show direction arrows, not wrap state

## Recommended Fix

Add visual feedback for wrap-around:

**Option 1** - Flash the position indicator:
```cpp
void InfoView::scroll(bool down) {
    if (totalLines_ <= VISIBLE_LINES) return;

    bool wasWrapped = false;
    if (down) {
        if (scrollLine_ < totalLines_ - VISIBLE_LINES) {
            scrollLine_++;
        } else {
            scrollLine_ = 0;
            wasWrapped = true;
        }
    } else {
        if (scrollLine_ > 0) {
            scrollLine_--;
        } else {
            scrollLine_ = totalLines_ - VISIBLE_LINES;
            wasWrapped = true;
        }
    }

    if (wasWrapped) {
        // Set a flag to indicate wrap for one render cycle
        wrapIndicator_ = true;
    }
    dirty_ = true;
}
```

**Option 2** - Show wrap indicator in footer:
```cpp
// In render(), when wrapIndicator_ is true, show a special symbol
if (wrapIndicator_) {
    prefix = "~WRAP~ ";  // Or use a unicode arrow symbol
    wrapIndicator_ = false;
}
```

## References

- `InfoView::scroll()` - `components/cdc_views/src/InfoView.cpp:81-99`
- `InfoView::render()` scroll indicators - `components/cdc_views/src/InfoView.cpp:218-220`
