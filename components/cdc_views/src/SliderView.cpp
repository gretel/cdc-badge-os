/**
 * SliderView Implementation
 *
 * Value adjustment with visual progress bar.
 */

#include "cdc_views/SliderView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <cstdio>
#include <algorithm>

static const char* TAG = "SliderView";

// Display constants
static constexpr int TITLE_Y = 20;
static constexpr int VALUE_Y = 55;
static constexpr int BAR_Y = 85;
static constexpr int BAR_HEIGHT = 20;
static constexpr int BAR_MARGIN = 20;

namespace cdc::ui {

void SliderView::init(const char* title, uint16_t minVal, uint16_t maxVal,
                      uint16_t initial, uint16_t step, const char* unit) {
    title_ = title;
    minValue_ = minVal;
    maxValue_ = maxVal;
    value_ = std::clamp(initial, minVal, maxVal);
    step_ = step > 0 ? step : 1;
    unit_ = unit;
    displayOffset_ = 0;
    zeroLabel_ = nullptr;
    dirty_ = true;
}

void SliderView::setValue(uint16_t value) {
    value = std::clamp(value, minValue_, maxValue_);
    if (value_ != value) {
        value_ = value;
        dirty_ = true;
    }
}

void SliderView::adjust(bool increase) {
    uint16_t newValue = value_;

    // Get step size (dynamic or fixed)
    uint16_t currentStep = stepCallback_ ? stepCallback_(value_, increase) : step_;

    if (increase) {
        uint16_t next = value_ + currentStep;
        newValue = (next > maxValue_) ? maxValue_ : next;
    } else {
        newValue = (value_ > currentStep) ? value_ - currentStep : minValue_;
    }

    if (newValue != value_) {
        value_ = newValue;
        dirty_ = true;
        LOG_D(TAG, "Slider adjusted to %d (step=%d)", value_, currentStep);

        // Call change callback for real-time updates (e.g., brightness preview)
        if (onChange_) {
            onChange_(value_);
        }
    }
}

InputResult SliderView::onKey(char key) {
    switch (key) {
        case '6': // Right = Increase
            adjust(true);
            return InputResult::CONSUMED;

        case '4': // Left = Decrease
            adjust(false);
            return InputResult::CONSUMED;

        case 'Y': // Save
            if (onSave_) {
                onSave_(value_);
            }
            return InputResult::REQUEST_POP;

        case 'N': // Cancel
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}

const char* SliderView::getFooterHint() const {
    return tr(StringId::HINT_BRIGHTNESS);
}

void SliderView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    const uint16_t width = display->getWidth();
    const uint16_t height = display->getHeight();

    if (!partial) {
        gfx->fillScreen(EPD_WHITE);
    }

    gfx->setTextColor(EPD_BLACK);

    // Title (centered)
    if (title_) {
        gfx->setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(title_, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((width - w) / 2, TITLE_Y);
        gfx->print(title_);
    }

    // Value display (centered, larger)
    char valueStr[32];
    int16_t displayValue = static_cast<int16_t>(value_) + displayOffset_;

    if (value_ == 0 && zeroLabel_) {
        // Special label for zero
        snprintf(valueStr, sizeof(valueStr), "%s", zeroLabel_);
    } else if (unit_) {
        snprintf(valueStr, sizeof(valueStr), "%d %s", displayValue, unit_);
    } else {
        snprintf(valueStr, sizeof(valueStr), "%d", displayValue);
    }

    gfx->setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(valueStr, 0, 0, &x1, &y1, &w, &h);
    gfx->fillRect(0, VALUE_Y - 5, width, h + 10, EPD_WHITE);
    gfx->setCursor((width - w) / 2, VALUE_Y);
    gfx->print(valueStr);

    int barWidth = width - 2 * BAR_MARGIN;
    gfx->drawRect(BAR_MARGIN, BAR_Y, barWidth, BAR_HEIGHT, EPD_BLACK);

    int fillWidth = 0;
    if (maxValue_ > minValue_) {
        fillWidth = (value_ - minValue_) * (barWidth - 4) / (maxValue_ - minValue_);
    }
    gfx->fillRect(BAR_MARGIN + 2, BAR_Y + 2, fillWidth, BAR_HEIGHT - 4, EPD_BLACK);

    // Draw horizontal arrow indicators [4] < ... > [6]
    gfx->setTextSize(1);

    // Left arrow and [4] label
    int arrowY = BAR_Y + BAR_HEIGHT / 2;
    gfx->fillTriangle(
        BAR_MARGIN - 15, arrowY,
        BAR_MARGIN - 5, arrowY - 5,
        BAR_MARGIN - 5, arrowY + 5,
        EPD_BLACK
    );
    gfx->setCursor(BAR_MARGIN - 17, BAR_Y + BAR_HEIGHT + 8);
    gfx->print("[4]");

    // Right arrow and [6] label
    int barRight = BAR_MARGIN + barWidth;
    gfx->fillTriangle(
        barRight + 15, arrowY,
        barRight + 5, arrowY - 5,
        barRight + 5, arrowY + 5,
        EPD_BLACK
    );
    gfx->setCursor(barRight + 5, BAR_Y + BAR_HEIGHT + 8);
    gfx->print("[6]");

    const char* hint = getFooterHint();
    if (hint) {
        gfx->fillRect(0, height - 16, width, 16, EPD_BLACK);
        gfx->setTextColor(EPD_WHITE);
        gfx->setCursor(4, height - 12);
        gfx->print(hint);
    }

    dirty_ = false;
}

// ============================================================================
// Convenience Function
// ============================================================================

static SliderView s_sharedSlider;

SliderView* showSlider(const char* title, uint16_t minVal, uint16_t maxVal,
                       uint16_t initial, uint16_t step, const char* unit,
                       SliderView::SaveCallback onSave,
                       SliderView::ChangeCallback onChange) {
    s_sharedSlider.init(title, minVal, maxVal, initial, step, unit);
    s_sharedSlider.setOnSave(onSave);
    if (onChange) {
        s_sharedSlider.setOnChange(onChange);
    }
    ViewStack::instance().push(&s_sharedSlider);
    return &s_sharedSlider;
}

} // namespace cdc::ui
