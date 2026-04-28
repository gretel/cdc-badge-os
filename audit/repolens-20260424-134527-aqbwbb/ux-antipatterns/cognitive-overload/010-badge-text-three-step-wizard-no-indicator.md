---
title: "[MEDIUM] Badge text editing wizard lacks progress indication"
severity: MEDIUM
domain: UI/UX
lens: cognitive-overload
labels:
  - "multi-step-flow"
  - "progress-indicator"
---

## Summary
The badge text editing flow in `components/cdc_os_ui/src/SettingsHandlers.cpp` uses a 3-step wizard (Name -> Info -> Info2) with no visible progress indicator. Users cannot see how many steps remain or their current position in the flow.

**File:** `components/cdc_os_ui/src/SettingsHandlers.cpp`
**Lines:** 190-260

## Impact
- **Uncertainty:** Users don't know if they're at the last step or have more coming
- **Disorientation:** After completing "Name", users might expect to be done but face two more inputs
- **No Escape Planning:** Without knowing total steps, users cannot decide whether to continue or cancel

## Evidence
The wizard state machine uses opaque step constants:

```cpp
// Line 24-28: Opaque step constants
static constexpr uint8_t BADGE_STEP_NONE = 0;
static constexpr uint8_t BADGE_STEP_NAME = 1;
static constexpr uint8_t BADGE_STEP_INFO = 2;
static constexpr uint8_t BADGE_STEP_INFO2 = 3;
static uint8_t s_badgeTextPendingStep = BADGE_STEP_NONE;

// Line 200-230: Step transitions without progress display
static void showBadgeTextStep(uint8_t step) {
    const char* title = nullptr;
    switch (step) {
        case BADGE_STEP_NAME:
            title = tr(StringId::NAME);
            // ...
            break;
        case BADGE_STEP_INFO:
            title = tr(StringId::INFO);
            // ...
            break;
        case BADGE_STEP_INFO2:
            title = tr(StringId::INFO2);
            // ...
            break;
    }
    showT9Input(title, initial, cb, LockScreenView::MAX_TEXT_LEN);
}

// Line 238-255: Each step just sets next step, no progress shown
static void onBadgeNameSave(const char* text) {
    s_badgeTextPendingStep = BADGE_STEP_INFO;  // No progress indicator
}
static void onBadgeInfoSave(const char* text) {
    s_badgeTextPendingStep = BADGE_STEP_INFO2;  // No progress indicator
}
```

The `T9InputView` shows `title_` but never indicates "Step 1 of 3" or similar.

## Recommended Fix
Add progress indication to each wizard step:

**Option 1 (Simplest):** Update titles to include step counter:
```cpp
static void showBadgeTextStep(uint8_t step) {
    char titleWithProgress[64];
    uint8_t current = step - BADGE_STEP_NONE;
    uint8_t total = 3;

    switch (step) {
        case BADGE_STEP_NAME:
            snprintf(titleWithProgress, sizeof(titleWithProgress), "%s (%d/%d)",
                     tr(StringId::NAME), current, total);
            break;
        case BADGE_STEP_INFO:
            snprintf(titleWithProgress, sizeof(titleWithProgress), "%s (%d/%d)",
                     tr(StringId::INFO), current, total);
            break;
        case BADGE_STEP_INFO2:
            snprintf(titleWithProgress, sizeof(titleWithProgress), "%s (%d/%d)",
                     tr(StringId::INFO2), current, total);
            break;
    }
    showT9Input(titleWithProgress, initial, cb, LockScreenView::MAX_TEXT_LEN);
}
```

**Option 2 (Better):** Use a dedicated wizard view with visual progress bar (similar to `PinChangeView` which already shows "1/3: Current PIN").

## References
- Nielsen Norman Group: [Multi-Step Forms](https://www.nngroup.com/articles/web-form-design/)
- Material Design: [Steppers](https://material.io/components/steppers)
- UX Pattern: Always show "X of Y" in multi-step flows
