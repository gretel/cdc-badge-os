---
title: "[LOW] Lockout countdown lacks visual progress indicator"
severity: LOW
domain: interaction-design
lens: form-ux
labels:
  - "audit:interaction-design/form-ux"
---

## Summary
In `PinEntryView.cpp:100-117`, when the badge is locked out, the remaining time is shown as text (e.g., "Locked out: 45s"). However, there is no visual progress indicator showing how much time remains, requiring users to read the exact number and mentally calculate remaining time.

## Impact
- Users must remember the countdown value or constantly re-read it
- No quick visual sense of "mostly done" vs "just started"
- Less engaging feedback during wait periods

## Evidence
**File: `components/cdc_views/src/PinEntryView.cpp:100-117`**
```cpp
void PinEntryView::onTick(uint32_t nowMs) {
    // ...
    if (lockedOut_) {
        static uint32_t lastUpdate = 0;
        if (nowMs - lastUpdate >= 1000) {
            lastUpdate = 1000;
            dirty_ = true;  // Triggers re-render
        }
    }
}
```

**File: `components/cdc_views/src/PinEntryView.cpp:275-290`**
```cpp
if (pm.isBadgeBlocked()) {
    uint32_t remainingMs = pm.getLockoutRemainingMs();
    uint32_t remainingSec = (remainingMs + 999) / 1000;
    snprintf(statusStr, sizeof(statusStr), "%s: %lus", tr(StringId::LOCKED_OUT), remainingSec);
}
```

Only text is shown, no progress bar or visual indicator.

## Recommended Fix
1. Add a simple progress bar (horizontal line that shrinks) below the lockout text
2. Or use a circular progress indicator around the PIN dots
3. Update the display every second to show the countdown visually

## References
- Nielsen Norman Group: [Progress Indicators](https://www.nngroup.com/articles/progress-indicators/)
- Material Design: [Progress Bars](https://m2.material.io/components/progress-indicators#linear-progress-indicators)
