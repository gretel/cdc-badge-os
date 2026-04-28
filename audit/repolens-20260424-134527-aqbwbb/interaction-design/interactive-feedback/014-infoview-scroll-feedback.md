---
title: "[LOW] InfoView scroll position feedback is minimal"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "scroll-feedback"
  - "position-indicator"
  - "navigation-hint"
---

## Summary
The `InfoView` component (`components/cdc_views/src/InfoView.cpp:179-220`) displays scrollable text content with scroll indicators, but the feedback about scroll position and available navigation is minimal. The scroll indicator arrows and position counter are present but not visually prominent enough to clearly communicate the scroll state.

## Impact
- **Position awareness**: Users may not easily understand where they are in the content
- **Navigation clarity**: The relationship between key presses (2/8) and scroll direction is not visually reinforced
- **Edge states**: Users may not know when they've reached the top or bottom of the content

## Evidence
File: `components/cdc_views/src/InfoView.cpp`, lines 179-220

```cpp
// Scroll indicators (if needed)
if (totalLines_ > VISIBLE_LINES) {
    int indicatorX = width - SCROLL_INDICATOR_WIDTH;
    int listHeight = VISIBLE_LINES * LINE_HEIGHT;
    render::drawScrollIndicator(gfx, indicatorX, TEXT_START_Y, listHeight,
                                totalLines_, VISIBLE_LINES, scrollLine_);
}

// Footer
char posStr[20];  // Max: "65535-65535/65535  \0"
const char* prefix = nullptr;
if (totalLines_ > VISIBLE_LINES) {
    snprintf(posStr, sizeof(posStr), "%u-%u/%u  ",
             scrollLine_ + 1,
             scrollLine_ + VISIBLE_LINES > totalLines_ ? totalLines_ : scrollLine_ + VISIBLE_LINES,
             totalLines_);
    prefix = posStr;
}
const char* hint = getFooterHint();
render::drawFooterBar(gfx, width, height, prefix, hint, true);
```

The scroll indicator is rendered by `drawScrollIndicator()` (in `RenderHelpers.cpp:88-135`), which shows:
- Up/down arrows when there's content to scroll
- A scrollbar thumb showing relative position
- A position counter in the footer (e.g., "1-5/20")

However, there's no:
- Visual distinction when at top/bottom edges
- Highlighting of which direction can be scrolled
- Dynamic hint text that changes based on scroll position

## Recommended Fix
Enhance the scroll position feedback:

**Option A - Edge highlighting**:
```cpp
void InfoView::render(bool partial) {
    // ... existing code ...
    
    // Scroll indicators with edge state
    if (totalLines_ > VISIBLE_LINES) {
        int indicatorX = width - SCROLL_INDICATOR_WIDTH;
        int listHeight = VISIBLE_LINES * LINE_HEIGHT;
        
        // Highlight top arrow if at top
        if (scrollLine_ == 0) {
            gfx->fillRect(indicatorX, TEXT_START_Y, SCROLL_INDICATOR_WIDTH, 8, EPD_DARKGREY);
        }
        
        // Highlight bottom arrow if at bottom
        if (scrollLine_ + VISIBLE_LINES >= totalLines_) {
            int bottomY = TEXT_START_Y + listHeight - 8;
            gfx->fillRect(indicatorX, bottomY, SCROLL_INDICATOR_WIDTH, 8, EPD_DARKGREY);
        }
        
        render::drawScrollIndicator(gfx, indicatorX, TEXT_START_Y, listHeight,
                                    totalLines_, VISIBLE_LINES, scrollLine_);
    }
}
```

**Option B - Dynamic footer hints**:
```cpp
const char* InfoView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    
    // Show different hints based on scroll position
    if (totalLines_ <= VISIBLE_LINES) {
        return tr(StringId::HINT_NO_SCROLL);  // "No scroll needed"
    } else if (scrollLine_ == 0) {
        return tr(StringId::HINT_AT_TOP);     // "[8] Scroll down"
    } else if (scrollLine_ + VISIBLE_LINES >= totalLines_) {
        return tr(StringId::HINT_AT_BOTTOM);  // "[2] Scroll up"
    } else {
        return tr(StringId::HINT_SCROLL_BACK); // "[2/8] Scroll [N] Back"
    }
}
```

**Option C - Enhanced scrollbar with direction arrows**:
```cpp
void render::drawScrollIndicator(Gdey029T94* gfx, int x, int y, int listHeight,
                                 uint16_t totalItems, uint16_t visibleItems,
                                 uint16_t scrollPos) {
    if (!gfx) return;
    if (totalItems <= visibleItems) return;

    gfx->fillRect(x, y, kScrollIndicatorWidth, listHeight, EPD_WHITE);

    const int midX = x + (kScrollIndicatorWidth / 2);
    const int leftX = x + 1;
    const int rightX = x + kScrollIndicator_width - 1;

    // Draw up arrow (always visible if scrollable)
    const int topY = y + 4;
    gfx->fillTriangle(
        midX, topY,
        leftX, topY + 6,
        rightX, topY + 6,
        scrollPos > 0 ? EPD_BLACK : EPD_DARKGREY  // Dimmed at top
    );

    // Draw down arrow (always visible if scrollable)
    const int arrowY = y + listHeight - 12;
    gfx->fillTriangle(
        midX, arrowY + 8,
        leftX, arrowY + 2,
        rightX, arrowY + 2,
        (scrollPos + visibleItems < totalItems) ? EPD_BLACK : EPD_DARKGREY  // Dimmed at bottom
    );

    // ... rest of scrollbar thumb code ...
}
```

**Option D - Add visual scroll progress bar**:
```cpp
// In InfoView::render() - add a thin progress bar at the top or bottom
void InfoView::render(bool partial) {
    // ... existing code ...
    
    // Add scroll progress bar at top
    if (totalLines_ > VISIBLE_LINES) {
        int progressHeight = 2;
        int progressWidth = width - 20;
        int progressX = 10;
        int progressY = 25;  // Just below title
        
        // Background
        gfx->fillRect(progressX, progressY, progressWidth, progressHeight, EPD_WHITE);
        gfx->drawRect(progressX, progressY, progressWidth, progressHeight, EPD_BLACK);
        
        // Progress fill
        int fillWidth = progressWidth * scrollLine_ / (totalLines_ - VISIBLE_LINES);
        gfx->fillRect(progressX + 1, progressY + 1, fillWidth, progressHeight - 2, EPD_BLACK);
    }
}
```

## References
- [WAI-ARIA Scrollable Regions](https://www.w3.org/WAI/ARIA/apg/patterns/landmarks/examples/general.html)
- [Scroll position indicators](https://www.nngroup.com/articles/scroll-position/)
- [E-Paper display optimization for UI elements](https://www.good-display.com/epaper-design-guide/)
