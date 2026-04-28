---
title: "[LOW] SliderView lacks press feedback during value adjustment"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "active-state"
  - "visual-feedback"
  - "key-press"
---

## Summary
The `SliderView` component (`components/cdc_views/src/SliderView.cpp:155-210`) shows the current slider value and a progress bar, but provides no visual feedback when the user presses a key to adjust the value. Unlike the ListView which shows selection, the Slider doesn't indicate when it's being actively adjusted.

## Impact
- **Input confirmation**: Users don't get immediate visual confirmation that their key press was registered
- **Feedback loop**: The adjustment happens, but without a visual "active" state, the connection between action and result is weaker
- **E-Paper considerations**: On E-Paper displays where updates may be slightly delayed, press feedback is especially important

## Evidence
File: `components/cdc_views/src/SliderView.cpp`, lines 100-110 (key handling)

```cpp
case '6': // Right = Increase
    adjust(true);
    return InputResult::CONSUMED;

case '4': // Left = Decrease
    adjust(false);
    return InputResult::CONSUMED;
```

File: `components/cdc_views/src/SliderView.cpp`, lines 155-210 (rendering)

```cpp
// Value display (centered, larger)
char valueStr[32];
int16_t displayValue = static_cast<int16_t>(value_) + displayOffset_;

// ... value rendering code ...

int barWidth = width - 2 * BAR_MARGIN;
gfx->drawRect(BAR_MARGIN, BAR_Y, barWidth, BAR_HEIGHT, EPD_BLACK);

int fillWidth = 0;
if (maxValue_ > minValue_) {
    fillWidth = (value_ - minValue_) * (barWidth - 4) / (maxValue_ - minValue_);
}
gfx->fillRect(BAR_MARGIN + 2, BAR_Y + 2, fillWidth, BAR_HEIGHT - 4, EPD_BLACK);
```

The slider renders the current value but has no concept of an "active" or "pressed" state. When a key is pressed, the value changes but there's no visual indicator of the adjustment action itself.

## Recommended Fix
Add a temporary visual indicator when the slider is being adjusted:

1. **Add a `pressed_` state flag**:
```cpp
// In SliderView.h
private:
    bool pressed_ = false;
    uint32_t pressTimeMs_ = 0;

// In SliderView.cpp - adjust() function
void SliderView::adjust(bool increase) {
    pressed_ = true;
    pressTimeMs_ = esp_timer_get_time() / 1000;
    // ... existing adjustment code ...
}

// In onTick()
void SliderView::onTick(uint32_t nowMs) {
    if (pressed_ && (nowMs - pressTimeMs_ > 200)) {
        pressed_ = false;  // Reset after 200ms
        dirty_ = true;
    }
}
```

2. **Visual feedback during press**:
```cpp
void SliderView::render(bool partial) {
    // ... existing code ...
    
    // Draw press indicator
    if (pressed_) {
        // Flash the bar or add a highlight
        gfx->fillRect(BAR_MARGIN, BAR_Y, barWidth, BAR_HEIGHT, EPD_BLACK);
        gfx->fillRect(BAR_MARGIN + 2, BAR_Y + 2, fillWidth, BAR_HEIGHT - 4, EPD_WHITE);
    } else {
        // Normal rendering
        gfx->drawRect(BAR_MARGIN, BAR_Y, barWidth, BAR_HEIGHT, EPD_BLACK);
        gfx->fillRect(BAR_MARGIN + 2, BAR_Y + 2, fillWidth, BAR_HEIGHT - 4, EPD_BLACK);
    }
}
```

3. **Alternative - Add directional arrow indicators**:
```cpp
// Show a small arrow pointing in the direction of change
if (lastAdjustmentWasIncrease) {
    // Draw right arrow next to value
} else {
    // Draw left arrow next to value
}
```

## References
- [WAI-ARIA Slider Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/slider/)
- E-Paper display interaction design patterns
