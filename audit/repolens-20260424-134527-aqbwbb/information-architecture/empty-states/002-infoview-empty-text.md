---
title: "[LOW] InfoView shows blank text area when content is empty"
severity: LOW
domain: ui-framework
lens: empty-states
labels:
  - "empty-state"
  - "infoview"
  - "text-display"
---

## Summary
The `InfoView` component in `components/cdc_views/src/InfoView.cpp` displays a completely blank white text area when the text content is empty (length 0). Only the title bar and footer are visible, leaving users confused about what should be displayed.

**Location:** `components/cdc_views/src/InfoView.cpp:177-210` (render function)

## Impact
When `InfoView` is used to display status information, help text, or details and the content happens to be empty (e.g., no GPG key configured, empty notes field), users see:
- A blank white space that looks like a rendering bug
- No indication of why the area is empty
- No guidance on what action to take

The GPG module uses `InfoView` for status display (line 388 in `GpgModule.cpp`):
```cpp
if (!gpg_get_status(&status)) {
    s_infoView.init(mstr(STR_STATUS), mstr(STR_NO_KEY));
    ui::ViewStack::instance().push(&s_infoView);
    return;
}
```

This is actually handled well because it passes "No key" text. However, if a module passes empty text, the blank state appears.

## Evidence
In `InfoView::render()` (lines 177-210):
```cpp
// Clear text area
gfx->fillRect(TEXT_MARGIN, TEXT_START_Y, textAreaWidth, textAreaHeight, EPD_WHITE);

// Render visible lines
if (textBuf_[0] != '\0') {  // <-- Only renders if text exists
    const char* lineStart = textBuf_;
    // ... renders lines ...
}
// If text is empty, nothing is drawn here
```

When text is empty, the area is just cleared to white with no fallback content.

## Recommended Fix
Add empty state text rendering when `textBuf_[0]` is empty:

```cpp
// Render visible lines
if (textBuf_[0] != '\0') {
    // ... existing rendering code ...
} else {
    // Empty state
    const char* emptyText = tr(StringId::EMPTY);
    int16_t x1, y1, textWidth, textHeight;
    gfx->getTextBounds(emptyText, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int textX = TEXT_MARGIN + (textAreaWidth - textWidth) / 2;
    int textY = TEXT_START_Y + (textAreaHeight - textHeight) / 2;
    gfx->setCursor(textX, textY);
    gfx->print(emptyText);
}
```

This ensures consistent empty state handling across all `InfoView` usages.

## References
- InfoView header: `components/cdc_views/include/cdc_views/InfoView.h`
- GPG status usage: `components/mod_gpg/src/GpgModule.cpp:385-407`
