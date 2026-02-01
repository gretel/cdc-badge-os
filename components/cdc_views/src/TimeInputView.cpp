/**
 * TimeInputView Implementation
 *
 * Time input with hour/minute fields.
 */

#include "cdc_views/TimeInputView.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <cstdio>

static const char* TAG = "TimeInputView";

// Display constants
static constexpr int TITLE_Y = 20;
static constexpr int TIME_Y = 55;
static constexpr int UNDERLINE_Y = TIME_Y + 25;
static constexpr int HINT_Y = 90;

namespace cdc::ui {

void TimeInputView::init(const char* title, uint8_t hour, uint8_t minute) {
    title_ = title;
    hour_ = (hour <= 23) ? hour : 0;
    minute_ = (minute <= 59) ? minute : 0;
    currentField_ = Field::HOUR;
    digitPos_ = 0;
    dirty_ = true;
}

void TimeInputView::nextField() {
    if (currentField_ == Field::HOUR) {
        currentField_ = Field::MINUTE;
        digitPos_ = 0;
        dirty_ = true;
    }
}

void TimeInputView::prevField() {
    if (currentField_ == Field::MINUTE) {
        currentField_ = Field::HOUR;
        digitPos_ = 0;
        dirty_ = true;
    }
}

void TimeInputView::clearField() {
    switch (currentField_) {
        case Field::HOUR:
            hour_ = 0;
            break;
        case Field::MINUTE:
            minute_ = 0;
            break;
    }
    digitPos_ = 0;
    dirty_ = true;
}

void TimeInputView::enterDigit(char digit) {
    uint8_t d = digit - '0';

    switch (currentField_) {
        case Field::HOUR: {
            if (digitPos_ == 0) {
                hour_ = d * 10;
                digitPos_ = 1;
            } else {
                hour_ = (hour_ / 10) * 10 + d;
                if (hour_ > 23) hour_ = 23;
                nextField();  // Auto-advance to minute
            }
            break;
        }
        case Field::MINUTE: {
            if (digitPos_ == 0) {
                minute_ = d * 10;
                digitPos_ = 1;
            } else {
                minute_ = (minute_ / 10) * 10 + d;
                if (minute_ > 59) minute_ = 59;
                digitPos_ = 0;  // Stay in minute field after completion
            }
            break;
        }
    }

    dirty_ = true;
}

InputResult TimeInputView::onKey(char key) {
    // Digit input (all 0-9 keys are digits, auto-advances between fields)
    if (key >= '0' && key <= '9') {
        enterDigit(key);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case 'N':  // Clear or cancel
            if (digitPos_ > 0 ||
                (currentField_ == Field::HOUR && hour_ > 0) ||
                (currentField_ == Field::MINUTE && minute_ > 0)) {
                clearField();
                return InputResult::CONSUMED;
            }
            return InputResult::REQUEST_POP;

        case 'Y':  // Confirm
            if (hour_ > 23) hour_ = 23;
            if (minute_ > 59) minute_ = 59;
            LOG_I(TAG, "Time confirmed: %02d:%02d", hour_, minute_);
            if (onConfirm_) {
                onConfirm_(hour_, minute_);
            }
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}

const char* TimeInputView::getFooterHint() const {
    return tr(StringId::HINT_TIME_INPUT);
}

void TimeInputView::render(bool partial) {
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

    if (title_) {
        gfx->setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(title_, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((width - w) / 2, TITLE_Y);
        gfx->print(title_);
    }

    char timeStr[12];
    snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);

    gfx->setTextSize(3);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    int startX = (width - w) / 2;
    gfx->setCursor(startX, TIME_Y);
    gfx->print(timeStr);

    gfx->fillRect(0, UNDERLINE_Y, width, 4, EPD_WHITE);

    int charWidth = 18;
    int underlineX = startX;
    int underlineW = 2 * charWidth;

    switch (currentField_) {
        case Field::HOUR:
            underlineX = startX;
            break;
        case Field::MINUTE:
            underlineX = startX + 5 * charWidth;
            break;
    }

    gfx->fillRect(underlineX, UNDERLINE_Y, underlineW, 3, EPD_BLACK);

    const char* hint = getFooterHint();
    if (hint) {
        gfx->fillRect(0, height - 16, width, 16, EPD_BLACK);
        gfx->setTextColor(EPD_WHITE);
        gfx->setCursor(4, height - 12);
        gfx->print(hint);
    }

    dirty_ = false;
}

} // namespace cdc::ui
