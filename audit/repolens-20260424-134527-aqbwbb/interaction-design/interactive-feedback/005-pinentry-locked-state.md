---
title: "[MEDIUM] PinEntryView locked-out state lacks clear visual indication"
severity: MEDIUM
domain: cdc_views
lens: interactive-feedback
labels:
  - "disabled-state"
  - "error-state"
  - "visual-distinction"
---

## Summary
The `PinEntryView` component (`components/cdc_views/src/PinEntryView.cpp:254-277`) renders PIN dots that look identical whether the view is active or locked out. The locked-out state is only communicated through text status below the dots, not through the main interactive element itself.

## Impact
- **State clarity**: Users may not immediately recognize that the PIN entry is disabled/locked
- **Feedback consistency**: The main interactive element (PIN dots) should visually reflect its disabled state
- **Error communication**: The locked-out state is a critical error state that needs more prominent visual communication

## Evidence
File: `components/cdc_views/src/PinEntryView.cpp`, lines 254-277

```cpp
for (uint8_t i = 0; i < maxLength_; i++) {
    int x = startX + i * PIN_DOT_SIZE;
    int y = PIN_Y;

    if (i < length_) {
        // Filled dot for entered digits
        gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
    } else {
        // Empty dot for remaining positions
        gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
    }
}
```

The dots are rendered the same way regardless of `lockedOut_` state. The lockout status is only shown in text below:
```cpp
if (pm.isBadgeBlocked()) {
    uint32_t remainingMs = pm.getLockoutRemainingMs();
    uint32_t remainingSec = (remainingMs + 999) / 1000;
    snprintf(statusStr, sizeof(statusStr), "%s: %lus", tr(StringId::LOCKED_OUT), remainingSec);
}
```

## Recommended Fix
Visually distinguish the locked-out state through the PIN dots:

1. **Cross out the dots** when locked out:
```cpp
if (lockedOut_) {
    // Draw crossed-out dots (X through each dot)
    gfx->drawLine(x, y, x + PIN_DOT_SIZE, y + PIN_DOT_SIZE, EPD_BLACK);
    gfx->drawLine(x + PIN_DOT_SIZE, y, x, y + PIN_DOT_SIZE, EPD_BLACK);
} else {
    // Normal dot rendering
    if (i < length_) {
        gfx->fillCircle(...);
    } else {
        gfx->drawCircle(...);
    }
}
```

2. **Add a lock icon** near the dots when locked out:
```cpp
if (lockedOut_) {
    // Draw a small lock icon above or beside the dots
    gfx->drawRect(centerX - 6, centerY - 8, 12, 10, EPD_BLACK);
    gfx->fillRect(centerX - 3, centerY - 8, 6, 4, EPD_BLACK);
}
```

3. **Gray out the dots** (use lighter pattern for E-Paper):
```cpp
if (lockedOut_) {
    // Draw dots with diagonal hatching pattern
    for (int j = 0; j < PIN_DOT_SIZE; j += 3) {
        gfx->drawLine(x + j, y, x + PIN_DOT_SIZE, y + j, EPD_BLACK);
    }
}
```

## References
- [WAI-ARIA Disabled States](https://www.w3.org/WAI/ARIA/apg/patterns/button/examples/button-disabled/)
- Error state design patterns for embedded interfaces
