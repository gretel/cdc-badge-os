---
title: "[LOW] No visual countdown for T9 input character commit timeout"
severity: LOW
domain: interaction-design
lens: interactive-feedback
labels:
  - "interactive-feedback"
---

## Summary
In T9InputView, when typing multi-tap text, characters are committed after a 2-second timeout, but there's no visual indication of this countdown. Users must wait or press another key to see the character committed.

**Location:** `components/cdc_views/src/T9InputView.cpp:100-135`

The timeout-based character commit happens silently without visual feedback about remaining time.

## Impact
- **Uncertainty**: Users don't know if the current character is "pending" or "committed"
- **Timing errors**: Users might press the next key too early or too late
- **Learning curve**: The timeout behavior is not intuitively discoverable

## Evidence
```cpp
// components/cdc_views/src/T9InputView.cpp:100-135
bool T9InputView::processKey(char key) {
    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    bool sameKey = (key == lastKey_);
    bool timeout = (now - lastPressMs_) > TIMEOUT_MS;  // 2000ms timeout

    if (sameKey && !timeout && len_ > 0) {
        // Cycle through characters for the same key
        charIndex_++;
        text_[len_ - 1] = getChar(key, charIndex_);
        cursorActive_ = true;  // Marks character as pending
    } else {
        // New key or timeout - commit previous and add new
        if (len_ < maxLen_) {
            text_[len_++] = getChar(key, 0);
            charIndex_ = 0;
            cursorActive_ = false;  // Character is committed
        }
    }
    // ...
}

// components/cdc_views/src/T9InputView.cpp:300-320
void T9InputView::render(bool partial) {
    // Shows cursorActive_ as inverted last character
    if (cursorActive_ && i == len_ - 1) {
        gfx->fillRect(x, y - 2, 8, 14, EPD_BLACK);  // Inverted background
        gfx->setTextColor(EPD_WHITE);
        gfx->print(text_[i]);
        gfx->setTextColor(EPD_BLACK);
    }
    // But no countdown shown
}
```

**Current behavior:**
- `cursorActive_` is true when a character is pending commit
- The pending character is shown inverted (white on black)
- After 2 seconds, it automatically commits (becomes normal)
- No visual countdown or timer shows how much time remains

## Recommended Fix
Add a visual countdown indicator for the timeout:

1. **Show a progress bar or timer next to the pending character**:
   ```cpp
   // In render(), when cursorActive_ is true:
   if (cursorActive_) {
       uint32_t elapsed = (esp_timer_get_time() / 1000ULL) - lastPressMs_;
       uint32_t remaining = TIMEOUT_MS - elapsed;
       float progress = (float)remaining / TIMEOUT_MS;
       
       // Draw progress bar under the pending character
       int barWidth = 10;
       int filledWidth = barWidth * progress;
       gfx->fillRect(cursorX, cursorY + 10, filledWidth, 2, EPD_BLACK);
       gfx->drawRect(cursorX, cursorY + 10, barWidth, 2, EPD_BLACK);
   }
   ```

2. **Alternative: Blinking cursor that slows down**:
   ```cpp
   // Toggle a blink state based on remaining time
   // Faster blink as timeout approaches
   ```

3. **Simpler: Show numeric countdown in footer**:
   ```cpp
   const char* T9InputView::getFooterHint() const {
       if (cursorActive_) {
           uint32_t elapsed = (esp_timer_get_time() / 1000ULL) - lastPressMs_;
           uint32_t remaining = (TIMEOUT_MS - elapsed) / 1000 + 1;
           static char hint[32];
           snprintf(hint, sizeof(hint), "Commit in %lus", remaining);
           return hint;
       }
       return tr(StringId::HINT_T9_INPUT);
   }
   ```

**Scope:** ~1 hour implementation
- Add countdown display in footer or as progress indicator
- Use `onTick()` to update countdown display
- Test readability on E-Paper display

## References
- T9 input patterns from legacy feature phones
- Timeout patterns in UI design (auto-save, auto-commit)
- Existing `onTick()` pattern for periodic updates
