---
title: "[LOW] No error boundary around custom item renderer callback"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "ui-framework"
---

## Summary
The `ListView::render()` function in `components/cdc_views/src/ListView.cpp:227-230` calls the custom item renderer callback without error isolation. A single renderer throwing during render will crash the entire view rendering cycle.

**Evidence:**
- `components/cdc_views/src/ListView.cpp:227-230`:
```cpp
bool handled = false;
if (itemRenderer_) {
    handled = itemRenderer_(gfx, item, itemIndex,
                            0, y, rowWidth, itemHeight_,
                            isSelected, itemRendererCtx_);  // No error boundary!
}
```

- Called from within the render loop for each visible item:
```cpp
for (uint8_t i = 0; i < visibleItems_; i++) {
    uint16_t itemIndex = scrollPos_ + i;
    // ...
    if (itemRenderer_) {
        handled = itemRenderer_(...);  // Could throw for each item!
    }
}
```

- Usage pattern (from modules):
```cpp
listView->setItemRenderer([](Gdey029T94* gfx, const ListItem& item,
                             uint16_t index, int x, int y, int w, int h,
                             bool selected, void* ctx) {
    // Custom rendering logic that could throw
    gfx->print(customData[index]);
    return true;
});
```

## Impact
- **Render crash**: Single buggy renderer stops all list rendering
- **Stale display**: User may see old data with no way to refresh
- **No error visibility**: Renderer errors not logged
- **Debug difficulty**: Hard to identify which renderer failed

## Recommended Fix
Add error boundary around item renderer callback:

```cpp
void ListView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    const uint16_t width = display->getWidth();
    const uint16_t height = display->getHeight();

    if (!partial) {
        gfx->fillScreen(EPD_WHITE);
    }

    gfx->setTextColor(EPD_BLACK);
    gfx->setTextSize(1);

    render::drawHeaderLeft(gfx, title_, ITEM_PADDING_X, TITLE_Y, width);

    const int rowWidth = width - SCROLL_INDICATOR_WIDTH;
    for (uint8_t i = 0; i < visibleItems_; i++) {
        uint16_t itemIndex = scrollPos_ + i;
        int y = LIST_START_Y + i * itemHeight_;

        gfx->fillRect(0, y, rowWidth, itemHeight_, EPD_WHITE);

        if (itemIndex >= itemCount_) continue;

        const ListItem& item = items_[itemIndex];
        bool isSelected = (itemIndex == selection_);

        if (isSelected) {
            gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
            gfx->setTextColor(EPD_WHITE);
        } else {
            gfx->setTextColor(EPD_BLACK);
        }

        bool handled = false;
        if (itemRenderer_) {
            try {
                handled = itemRenderer_(gfx, item, itemIndex,
                                        0, y, rowWidth, itemHeight_,
                                        isSelected, itemRendererCtx_);
            } catch (const std::exception& e) {
                LOG_E(TAG, "Item renderer exception: %s", e.what());
                handled = false;  // Fall back to default rendering
            } catch (...) {
                LOG_E(TAG, "Item renderer exception (unknown)");
                handled = false;
            }
        }

        if (!handled) {
            // Default rendering logic...
        }
    }

    // ... rest of render
    dirty_ = false;
}
```

## References
- [UI Render Patterns](https://opencode.ai/guides/ui/render-patterns/)
- [Callback Error Handling](https://en.cppreference.com/w/cpp/utility/functional)
