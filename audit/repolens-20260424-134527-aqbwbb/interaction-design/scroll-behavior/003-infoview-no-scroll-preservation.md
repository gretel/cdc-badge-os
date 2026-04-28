---
title: "[LOW] InfoView lacks scroll position preservation for back-navigation"
severity: LONG
domain: interaction-design
lens: scroll-behavior
labels:
  - "scroll-restoration"
  - "info-view"
---

## Summary

`InfoView` displays scrollable text content (e.g., help screens, about pages) but has no mechanism to preserve scroll position when the user navigates back to it. When a user scrolls down in an info view, navigates to another view, and then returns (via back navigation or re-opening), the view always resets to the top.

**Evidence** (`components/cdc_views/src/InfoView.cpp:35-57`):
```cpp
void InfoView::init(const char* title, const char* text) {
    // Copy title to internal buffer
    if (title) {
        strncpy(titleBuf_, title, MAX_TITLE_LEN - 1);
        titleBuf_[MAX_TITLE_LEN - 1] = '\0';
    } else {
        titleBuf_[0] = '\0';
    }

    // Copy text to internal buffer
    if (text) {
        strncpy(textBuf_, text, MAX_TEXT_LEN - 1);
        textBuf_[MAX_TEXT_LEN - 1] = '\0';
    } else {
        textBuf_[0] = '\0';
    }

    scrollLine_ = 0;  // Always resets to top
    totalLines_ = countLines();
    customHint_ = nullptr;
    dirty_ = true;
    ...
}
```

## Impact

- **Reading flow disruption**: For long help texts or documentation (up to 2048 chars / ~150 lines), users must re-scroll to their previous position after navigating away and back.
- **Inconsistency**: `ListView` has `preservePosition()` support, but `InfoView` does not.

## Evidence

1. `components/cdc_views/src/InfoView.cpp:52` - `scrollLine_ = 0` always set in `init()`
2. `components/cdc_views/include/cdc_views/InfoView.h:60` - No `preservePosition()` method
3. `VISIBLE_LINES = 5` with potentially 100+ lines means users frequently scroll

## Recommended Fix

Add scroll position preservation to `InfoView`:

```cpp
// In InfoView.h
class InfoView : public ViewBase {
public:
    // ... existing methods ...
    
    void preservePosition() { preservePosition_ = true; }
    
private:
    bool preservePosition_ = false;
    // ... rest of class ...
};

// In InfoView.cpp
void InfoView::init(const char* title, const char* text) {
    if (title) {
        strncpy(titleBuf_, title, MAX_TITLE_LEN - 1);
        titleBuf_[MAX_TITLE_LEN - 1] = '\0';
    } else {
        titleBuf_[0] = '\0';
    }

    if (text) {
        strncpy(textBuf_, text, MAX_TEXT_LEN - 1);
        textBuf_[MAX_TEXT_LEN - 1] = '\0';
    } else {
        textBuf_[0] = '\0';
    }

    totalLines_ = countLines();
    
    if (!preservePosition_) {
        scrollLine_ = 0;
    } else {
        preservePosition_ = false;
        // Clamp scroll position to new line count
        uint16_t maxScroll = totalLines_ > VISIBLE_LINES ? totalLines_ - VISIBLE_LINES : 0;
        if (scrollLine_ > maxScroll) {
            scrollLine_ = maxScroll;
        }
    }
    
    customHint_ = nullptr;
    dirty_ = true;
    ...
}
```

## References

- `InfoView::init()` - `components/cdc_views/src/InfoView.cpp:35-57`
- `ListView::preservePosition()` for reference - `components/cdc_views/include/cdc_views/ListView.h:124`
