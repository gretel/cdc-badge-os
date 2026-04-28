---
title: "[LOW] PIN entry lockout countdown updates but lacks clear visual distinction"
severity: LOW
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When the badge is locked out after failed PIN attempts (`components/cdc_views/src/PinEntryView.cpp:90-116`), the lockout timer counts down every second and updates the display, but the **locked state is not visually distinct enough**. The same view is re-rendered with just the text updated, making it hard for users to immediately recognize they're in a "locked" state vs. just entering a PIN.

**Evidence:**
- `PinEntryView::onTick()` (line 90-116) updates `dirty_` every second during lockout
- The lockout message is shown via `showMessage()` but auto-dismisses after 3 seconds
- After dismissal, only the status text changes - no persistent visual indicator (e.g., red overlay, dimmed state)
- The view continues to look like a normal PIN entry, just with lockout text

## Impact
**User Experience:** After the initial 3-second error toast dismisses, users may not clearly see they're in a locked state. The countdown text is small and may be missed. A user could try to enter a PIN again, not realizing the view is waiting for the timeout.

**Technical:** The countdown works correctly, but the visual hierarchy doesn't emphasize the "locked" state enough.

## Evidence
**File: `components/cdc_views/src/PinEntryView.cpp`**

```cpp
// Line 90-116: Lockout countdown with minimal visual feedback
void PinEntryView::onTick(uint32_t nowMs) {
    (void)nowMs;
    core::PinManager& pm = core::PinManager::instance();

    // Check if blocked status changed
    bool blocked = pm.isBadgeBlocked();
    if (blocked != lockedOut_) {
        lockedOut_ = true;
        dirty_ = true;
    }

    // Update display every second during lockout to show countdown
    if (lockedOut_) {
        static uint32_t lastUpdate = 0;
        if (nowMs - lastUpdate >= 1000) {
            lastUpdate = nowMs;
            dirty_ = true;  // Just re-renders the same view with different text
        }
    }
}

// Line 125-165: Lockout message shown then auto-dismisses
void PinEntryView::verify() {
    // ...
    if (pm.isBadgeBlocked()) {
        lockedOut_ = true;
        if (showMessages_) {
            showMessage(tr(StringId::LOCKED_OUT), MessageIcon::ERROR, 3000);  // Dismisses after 3s
        }
    }
    // ...
}
```

**File: `components/cdc_views/src/PinEntryView.cpp:218-299` (render method)**
- The render method shows the same layout regardless of locked state
- Only the text changes from "Retries: 3" to "LOCKED OUT: 45s"
- No visual distinction like dimming, overlay, or color change

## Recommended Fix
Add a persistent visual indicator for the locked state:

```cpp
void PinEntryView::render(bool partial) {
    // ... existing code ...

    // Draw locked-out overlay (semi-transparent darkening)
    if (lockedOut_) {
        gfx->fillRect(0, 0, width, height, EPD_BLACK);  // Dark background
        // Re-draw content in white
        gfx->setTextColor(EPD_WHITE);
        // ... redraw all elements with inverted colors ...
    } else {
        gfx->setTextColor(EPD_BLACK);
        // ... normal rendering ...
    }

    // Show lockout countdown prominently
    if (lockedOut_) {
        // Draw large countdown timer in center
        gfx->setTextSize(3);  // Larger font
        gfx->setCursor((width - w) / 2, 60);
        gfx->print(statusStr);  // "LOCKED OUT: 45s"
    }
}
```

## References
- Lockout states should be visually distinct from normal states
- Consider using the MessageBox overlay for persistent lockout state instead of auto-dismissing toast
