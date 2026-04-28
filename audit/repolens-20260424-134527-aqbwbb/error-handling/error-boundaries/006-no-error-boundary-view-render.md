---
title: "[MEDIUM] No error boundary around view render methods"
severity: MEDIUM
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "ui-framework"
---

## Summary
The `ViewStack::render()` function in `components/cdc_ui/src/ViewStack.cpp:237-268` calls view render methods without error isolation. A single view throwing during render will crash the entire UI refresh cycle.

**Evidence:**
- `components/cdc_ui/src/ViewStack.cpp:237-268`:
```cpp
void ViewStack::render() {
    IView* view = current();
    if (!view) {
        return;
    }

    // Check if rendering is needed
    bool modalNeedsRender = modal_ && modal_->needsRender();
    bool viewNeedsRender = view->needsRender();

    if (!viewNeedsRender && !modalNeedsRender) {
        return;
    }

    // Render current view (always full, not partial)
    if (viewNeedsRender) {
        view->render(false);  // No error boundary!
    }

    // Render modal on top
    if (modal_ && modalNeedsRender) {
        modal_->render(true);  // No error boundary!
    }

    // Flush display
    hal::IDisplay* display = hal::getDisplayInstance();
    if (display) {
        display->flush(mode);
    }
    needsFullRefresh_ = false;
}
```

## Impact
- **Display freeze**: Single render failure stops all UI updates
- **Stale display**: User may see old data with no way to refresh
- **No visual feedback**: Errors during render not logged or shown

## Recommended Fix
Add error boundaries around render calls:

```cpp
void ViewStack::render() {
    IView* view = current();
    if (!view) {
        return;
    }

    bool modalNeedsRender = modal_ && modal_->needsRender();
    bool viewNeedsRender = view->needsRender();

    if (!viewNeedsRender && !modalNeedsRender) {
        return;
    }

    // Render current view with error boundary
    if (viewNeedsRender) {
        try {
            view->render(false);
        } catch (const std::exception& e) {
            LOG_E(TAG, "View render exception: %s", e.what());
        } catch (...) {
            LOG_E(TAG, "View render exception (unknown)");
        }
    }

    // Render modal with error boundary
    if (modal_ && modalNeedsRender) {
        try {
            modal_->render(true);
        } catch (const std::exception& e) {
            LOG_E(TAG, "Modal render exception: %s", e.what());
        } catch (...) {
            LOG_E(TAG, "Modal render exception (unknown)");
        }
    }

    hal::IDisplay* display = hal::getDisplayInstance();
    if (display) {
        display->flush(mode);
    }
    needsFullRefresh_ = false;
}
```

## References
- [UI Render Patterns](https://opencode.ai/guides/ui/render-patterns/)
- [Display Refresh Best Practices](https://en.cppreference.com/w/cpp/language/virtual)
