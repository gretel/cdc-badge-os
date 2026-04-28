---
title: "[LOW] T9InputView cursor feedback is inconsistent"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "cursor-state"
  - "input-feedback"
  - "text-input"
---

## Summary
The `T9InputView` component (`components/cdc_views/src/T9InputView.cpp:297-325`) shows cursor feedback in two different ways: an inverted block for the last character (when actively typing) and a pipe `|` character (when idle). This inconsistency can confuse users about whether they are in "typing mode" or "idle mode".

## Impact
- **Visual consistency**: Two different cursor styles for what is essentially the same state
- **User confusion**: Users may not understand when the cursor is "active" vs "idle"
- **Multi-tap timing**: The cursor behavior is tied to the T9 timeout, which may not be intuitive

## Evidence
File: `components/cdc_views/src/T9InputView.cpp`, lines 297-325

```cpp
// Show text with cursor
for (uint16_t i = 0; i < len_; i++) {
    // If this is the last char and cursor is active, invert it
    if (cursorActive_ && i == len_ - 1) {
        int16_t x = gfx->getCursorX();
        int16_t y = gfx->getCursorY();
        gfx->fillRect(x, y - 2, 8, 14, EPD_BLACK);
        gfx->setTextColor(EPD_WHITE);
        gfx->print(text_[i]);
        gfx->setTextColor(EPD_BLACK);
    } else {
        gfx->print(text_[i]);
    }
}

// Show cursor at end if not in T9 cycle
if (!cursorActive_) {
    gfx->print("|");
}
```

The cursor behavior:
1. **Active cursor** (`cursorActive_ = true`): Last character is inverted (white on black background)
2. **Idle cursor** (`cursorActive_ = false`): A `|` pipe character is appended after the text

The `cursorActive_` flag is set to `true` when typing and cleared after a timeout or when a new key is pressed. This creates two visually distinct cursor states.

## Recommended Fix
Standardize on a single cursor style:

**Option A - Use block cursor throughout**:
```cpp
// Always show block cursor at end of text
int16_t x = gfx->getCursorX();
int16_t y = gfx->getCursorY();

// Print all text
for (uint16_t i = 0; i < len_; i++) {
    gfx->print(text_[i]);
}

// Add block cursor at end
gfx->fillRect(x, y - 2, 8, 14, EPD_BLACK);
gfx->setTextColor(EPD_WHITE);
gfx->print(" ");  // Space character in inverted block
gfx->setTextColor(EPD_BLACK);
```

**Option B - Use pipe cursor throughout with blink effect**:
```cpp
// Toggle cursor visibility periodically
static uint32_t lastBlinkMs = 0;
static bool cursorVisible = true;
if (nowMs - lastBlinkMs > 500) {  // Blink every 500ms
    lastBlinkMs = nowMs;
    cursorVisible = !cursorVisible;
}

// Print text
for (uint16_t i = 0; i < len_; i++) {
    gfx->print(text_[i]);
}

// Add pipe cursor if visible
if (cursorVisible) {
    gfx->print("|");
}
```

**Option C - Show mode indicator**:
If keeping both cursor styles, add a clear indicator of what each means:
```cpp
// In footer hint, show current mode
if (cursorActive_) {
    hint = tr(StringId::T9_INPUT_ACTIVE);  // "Press another key or wait..."
} else {
    hint = tr(StringId::T9_INPUT_IDLE);    // "Continue typing..."
}
```

## References
- [Text input cursor design patterns](https://www.nngroup.com/articles/text-entry/)
- Multi-tap input UX considerations
