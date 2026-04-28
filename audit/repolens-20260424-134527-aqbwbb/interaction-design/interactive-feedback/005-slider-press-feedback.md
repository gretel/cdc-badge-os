---
title: "[LOW] Slider adjustment lacks immediate visual confirmation"
severity: LOW
domain: interaction-design
lens: interactive-feedback
labels:
  - "interactive-feedback"
---

## Summary
When adjusting the SliderView with keys 4/6, the value changes but there's no visual "active" feedback to confirm the key press was registered before the next render cycle.

**Location:** `components/cdc_views/src/SliderView.cpp:95-115`

The adjustment happens immediately but without any visual indicator that the action was triggered.

## Impact
- **Uncertain feedback**: On E-Paper displays, users may not know if their key press was registered
- **Multiple presses**: Users might press the key multiple times wondering if the first press worked
- **Poor perceived responsiveness**: Lack of immediate visual confirmation

## Evidence
```cpp
// components/cdc_views/src/SliderView.cpp:95-115
InputResult SliderView::onKey(char key) {
    switch (key) {
        case '6': // Right = Increase
            adjust(true);  // Value changes immediately
            return InputResult::CONSUMED;

        case '4': // Left = Decrease
            adjust(false);  // Value changes immediately
            return InputResult::CONSUMED;
        // ...
    }
}

void SliderView::adjust(bool increase) {
    uint16_t newValue = value_;
    uint16_t currentStep = stepCallback_ ? stepCallback_(value_, increase) : step_;

    if (increase) {
        uint16_t next = value_ + currentStep;
        newValue = (next > maxValue_) ? maxValue_ : next;
    } else {
        newValue = (value_ > currentStep) ? value_ - currentStep : minValue_;
    }

    if (newValue != value_) {
        value_ = newValue;
        dirty_ = true;  // Marks for re-render
        if (onChange_) {
            onChange_(value_);  // Callback for real-time updates
        }
    }
}
```

**Current behavior:**
- Key press triggers `adjust()` immediately
- `dirty_` is set to true (will re-render)
- Value changes and `onChange_` callback fires
- No visual "pressed" state for the adjustment action
- Next render shows new value but no confirmation of the action itself

## Recommended Fix
Add visual feedback for the adjustment action:

1. **Flash the value display briefly**:
   ```cpp
    void SliderView::adjust(bool increase) {
    // ... existing code ...
    
    if (newValue != value_) {
        value_ = newValue;
        dirty_ = true;
        pressed_ = true;  // Track for visual feedback
        if (onChange_) {
            onChange_(value_);
        }
    }
}

// In render():
if (pressed_) {
    // Draw value with different style (e.g., bolder, different size)
    gfx->setTextSize(3);  // Make it bigger momentarily
    // ... render value ...
}
```

2. **Add a brief "pulse" animation effect**:
   ```cpp
    // In render(), when pressed_ is true:
    // Draw a border around the value or flash the bar
    if (pressed_) {
    gfx->drawRect(BAR_MARGIN - 2, BAR_Y - 2, barWidth + 4, BAR_HEIGHT + 4, EPD_BLACK);
}
```

3. **Show arrow indicators temporarily highlighted**:
   ```cpp
    // In render(), highlight the [4] or [6] key hint based on direction
    if (pressed_ && lastDirection_ == INCREASE) {
    // Highlight [6] indicator
    gfx->fillRect(..., EPD_BLACK);  // Invert the hint area
}
```

**Scope:** ~1 hour implementation
- Add `pressed_` and `lastDirection_` state variables
- Modify `adjust()` to set these variables
- Update `render()` to show visual feedback
- Consider auto-clearing `pressed_` after one render cycle

## References
- Material Design: Ripple effect for button presses
- E-Paper display refresh optimization
- Existing `dirty_` flag pattern for render scheduling
