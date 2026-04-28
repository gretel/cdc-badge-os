---
title: "[LOW] RGB input view requires numeric entry without visual color picker"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - form-fatigue
  - input-complexity
---

## Summary

The `RgbInputView` component (`RgbInputView.cpp:1-274`) requires users to enter RGB values as 3-digit numbers (0-255) for each color channel. While a preview box is shown, users must mentally calculate or guess the numeric values for desired colors, creating cognitive overhead.

**Files:**
- `components/grove_led/src/RgbInputView.cpp:1-274` (RGB input implementation)
- `components/grove_led/include/grove_led/GroveLedModule.h:45-55` (LED settings)

## Impact

**User Experience:** Users must enter 9 digits total (3 per channel) to set a color. The preview box uses grayscale approximation on the e-ink display, reducing its usefulness. Users cannot intuitively select colors without knowing the exact RGB values.

**Evidence:**
From `RgbInputView.cpp:105-120`, the digit entry logic:
```cpp
void RgbInputView::enterDigit(char digit) {
    uint8_t d = digit - '0';
    uint8_t* value = nullptr;

    switch (currentField_) {
        case Field::RED:   value = &r_; break;
        case Field::GREEN: value = &g_; break;
        case Field::BLUE:  value = &b_; break;
    }

    if (digitPos_ == 0) {
        // First digit (hundreds place)
        *value = d * 100;
        digitPos_ = 1;
    } else if (digitPos_ == 2) {
        // Third digit - clamp and auto-advance
        uint8_t base = (*value / 10) * 10;
        *value = (d > 255 - base) ? 255 : (base + d);
        nextField();
    }
}
```

The preview box (line 250-255) uses grayscale approximation, limiting its utility for color selection.

## Recommended Fix

1. **Add preset color options** (common colors like red, green, blue, yellow, etc.)
2. **Implement slider-based input** for each channel (0-255) with up/down navigation
3. **Add color wheel navigation** if display size permits
4. **Show a color swatch** with better grayscale approximation or dithering

Quick fix: Add a preset color menu that can be accessed via a key (e.g., 'P' for presets) to select common colors without manual entry.

## References

- Nielsen Norman Group: "Color Pickers: Design Patterns for Different Platforms"
- Material Design: "Color selection" patterns
