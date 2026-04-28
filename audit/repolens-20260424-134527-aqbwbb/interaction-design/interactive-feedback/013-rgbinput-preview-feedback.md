---
title: "[LOW] RgbInputView preview box lacks dynamic feedback during adjustment"
severity: LOW
domain: grove_led
lens: interactive-feedback
labels:
  - "active-state"
  - "preview-feedback"
  - "visual-update"
---

## Summary
The `RgbInputView` component (`components/grove_led/src/RgbInputView.cpp:250-260`) displays a preview box showing the current RGB color, but the preview only updates after a digit is fully entered. There is no visual feedback during the multi-digit input process, which can make users uncertain if their input is being registered.

## Impact
- **Input uncertainty**: Users typing 3 digits for each color component don't get immediate visual confirmation
- **Feedback delay**: The preview box updates only after field completion, not during input
- **E-Paper considerations**: On E-Paper displays, delayed feedback can be especially disorienting

## Evidence
File: `components/grove_led/src/RgbInputView.cpp`, lines 90-120 (digit entry)

```cpp
void RgbInputView::enterDigit(char digit) {
    uint8_t d = digit - '0';
    uint8_t* value = nullptr;

    switch (currentField_) {
        case Field::RED:   value = &r_; break;
        case Field::GREEN: value = &g_; break;
        case Field::BLUE:  value = &b_; break;
    }

    if (!value) return;

    if (digitPos_ == 0) {
        // First digit
        *value = d * 100;
        digitPos_ = 1;
    } else if (digitPos_ == 1) {
        // Second digit
        *value = (*value / 100) * 100 + d * 10;
        digitPos_ = 2;
    } else {
        // Third digit - clamp and auto-advance
        uint8_t base = (*value / 10) * 10;
        *value = (d > 255 - base) ? 255 : (base + d);
        nextField();
    }

    dirty_ = true;
}
```

File: `components/grove_led/src/RgbInputView.cpp`, lines 250-260 (preview rendering)

```cpp
// Color preview box
gfx->setTextSize(1);
gfx->setCursor(10, PREVIEW_Y);
gfx->print("Preview:");

// Draw preview rectangle (grayscale approximation for e-ink)
uint8_t luminance = (r_ * 77 + g_ * 150 + b_ * 29) >> 8;
gfx->fillRect(70, PREVIEW_Y - 4, 50, 14, EPD_BLACK);
if (luminance > 127) {
    gfx->fillRect(72, PREVIEW_Y - 2, 46, 10, EPD_WHITE);
}
```

The preview box renders based on the current `r_`, `g_`, `b_` values, which are updated during digit entry. However:
- No visual indicator that the preview is "active" or "being adjusted"
- No intermediate feedback as digits are entered (e.g., partial preview)
- The underlined field shows which component is being edited, but the preview doesn't reflect this

## Recommended Fix
Add visual feedback to indicate the preview is being adjusted:

**Option A - Flash the preview during input**:
```cpp
// In RgbInputView.h
private:
    bool previewFlash_ = false;
    uint32_t flashTime_ = 0;

// In enterDigit()
void RgbInputView::enterDigit(char digit) {
    // ... existing code ...
    
    previewFlash_ = true;
    flashTime_ = esp_timer_get_time() / 1000;
    dirty_ = true;
}

// In onTick()
void RgbInputView::onTick(uint32_t nowMs) {
    if (previewFlash_ && (nowMs - flashTime_ > 150)) {
        previewFlash_ = false;
        dirty_ = true;
    }
}

// In render()
void RgbInputView::render(bool partial) {
    // ... existing code ...
    
    // Draw preview with flash effect
    uint8_t luminance = (r_ * 77 + g_ * 150 + b_ * 29) >> 8;
    if (previewFlash_) {
        // Invert colors temporarily
        gfx->fillRect(70, PREVIEW_Y - 4, 50, 14, EPD_WHITE);
        gfx->drawRect(70, PREVIEW_Y - 4, 50, 14, EPD_BLACK);
        if (luminance > 127) {
            gfx->fillRect(72, PREVIEW_Y - 2, 46, 10, EPD_BLACK);
        }
    } else {
        // Normal rendering
        gfx->fillRect(70, PREVIEW_Y - 4, 50, 14, EPD_BLACK);
        if (luminance > 127) {
            gfx->fillRect(72, PREVIEW_Y - 2, 46, 10, EPD_WHITE);
        }
    }
}
```

**Option B - Show partial preview updates**:
```cpp
// Show a "dim" preview for incomplete values
void RgbInputView::render(bool partial) {
    // ... existing code ...
    
    // Calculate luminance
    uint8_t luminance = (r_ * 77 + g_ * 150 + b_ * 29) >> 8;
    
    // Check if current field is incomplete
    bool isIncomplete = (digitPos_ < 2);  // Less than 2 digits entered
    
    if (isIncomplete) {
        // Draw preview with lighter shade (hatched pattern for E-Paper)
        gfx->fillRect(70, PREVIEW_Y - 4, 50, 14, EPD_WHITE);
        for (int i = 0; i < 50; i += 3) {
            gfx->drawLine(70 + i, PREVIEW_Y - 4, 70 + i + 3, PREVIEW_Y + 10, EPD_BLACK);
        }
    } else {
        // Normal preview
        gfx->fillRect(70, PREVIEW_Y - 4, 50, 14, EPD_BLACK);
        if (luminance > 127) {
            gfx->fillRect(72, PREVIEW_Y - 2, 46, 10, EPD_WHITE);
        }
    }
}
```

**Option C - Add a border highlight to the active field's portion of the preview**:
```cpp
// Split the preview into 3 sections (R, G, B) and highlight the active one
void RgbInputView::render(bool partial) {
    // ... existing code ...
    
    int previewX = 70;
    int previewY = PREVIEW_Y - 4;
    int sectionWidth = 16;
    
    // Draw 3 sections
    for (int i = 0; i < 3; i++) {
        int sectionX = previewX + i * sectionWidth;
        if (static_cast<int>(currentField_) == i) {
            // Highlight active section
            gfx->fillRect(sectionX, previewY, sectionWidth, 14, EPD_BLACK);
            gfx->drawRect(sectionX - 1, previewY - 1, sectionWidth + 2, 16, EPD_WHITE);
        } else {
            gfx->fillRect(sectionX, previewY, sectionWidth, 14, EPD_BLACK);
        }
    }
}
```

## References
- [WAI-ARIA Live Regions](https://www.w3.org/WAI/WCAG21/Techniques/aria/aria-live) - For real-time updates
- [Input feedback patterns](https://www.nngroup.com/articles/input-feedback/)
- E-Paper display interaction design patterns
