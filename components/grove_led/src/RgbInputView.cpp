/**
 * RgbInputView Implementation
 *
 * RGB color input with R/G/B fields.
 */

#include "grove_led/RgbInputView.h"
#include "cdc_views/KeyCodes.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <cstdio>

static const char* TAG = "RgbInputView";

/**
 * \brief Layout constants for RGB input rendering.
 */
static constexpr int TITLE_Y = 20;
static constexpr int RGB_Y = 55;
static constexpr int UNDERLINE_Y = RGB_Y + 20;
static constexpr int PREVIEW_Y = 95;
static constexpr int HINT_Y = 115;

namespace cdc::grove_led {

/**
 * \brief Initializes RGB input view state.
 * \param title View title.
 * \param r Initial red value.
 * \param g Initial green value.
 * \param b Initial blue value.
 */
void RgbInputView::init(const char* title, uint8_t r, uint8_t g, uint8_t b) {
    title_ = title;
    r_ = r;
    g_ = g;
    b_ = b;
    currentField_ = Field::RED;
    digitPos_ = 0;
    dirty_ = true;
}

/**
 * \brief Moves selection to next RGB field.
 */
void RgbInputView::nextField() {
    if (currentField_ == Field::RED) {
        currentField_ = Field::GREEN;
    } else if (currentField_ == Field::GREEN) {
        currentField_ = Field::BLUE;
    }
    digitPos_ = 0;
    dirty_ = true;
}

/**
 * \brief Moves selection to previous RGB field.
 */
void RgbInputView::prevField() {
    if (currentField_ == Field::BLUE) {
        currentField_ = Field::GREEN;
    } else if (currentField_ == Field::GREEN) {
        currentField_ = Field::RED;
    }
    digitPos_ = 0;
    dirty_ = true;
}

/**
 * \brief Clears currently selected RGB field.
 */
void RgbInputView::clearField() {
    switch (currentField_) {
        case Field::RED:
            r_ = 0;
            break;
        case Field::GREEN:
            g_ = 0;
            break;
        case Field::BLUE:
            b_ = 0;
            break;
    }
    digitPos_ = 0;
    dirty_ = true;
}

/**
 * \brief Processes one numeric digit for current RGB field.
 * \param digit ASCII digit character.
 */
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
        uint16_t staged = static_cast<uint16_t>(d) * 100u;
        *value = static_cast<uint8_t>(staged > 255u ? 255u : staged);
        digitPos_ = 1;
    } else if (digitPos_ == 1) {
        uint16_t staged = static_cast<uint16_t>((*value / 100u) * 100u + d * 10u);
        *value = static_cast<uint8_t>(staged > 255u ? 255u : staged);
        digitPos_ = 2;
    } else {
        uint16_t base = static_cast<uint16_t>((*value / 10u) * 10u);
        uint16_t staged = static_cast<uint16_t>(base + d);
        *value = static_cast<uint8_t>(staged > 255u ? 255u : staged);
        nextField();
    }

    dirty_ = true;
}

/**
 * \brief Clamps RGB values to supported range (kept for API symmetry).
 */
void RgbInputView::clampValues() {
    // uint8_t values cannot exceed 255, function kept for API consistency
}

/**
 * \brief Handles keypad input for RGB editor workflow.
 * \param key Pressed key.
 * \return Input processing result.
 */
ui::InputResult RgbInputView::onKey(char key) {
    // Digit input
    if (key >= '0' && key <= '9') {
        enterDigit(key);
        return ui::InputResult::CONSUMED;
    }

    switch (key) {
        case '4':  // Previous field
            prevField();
            return ui::InputResult::CONSUMED;

        case '6':  // Next field
            nextField();
            return ui::InputResult::CONSUMED;

        case ui::KEY_NO:  // Clear or cancel
            if (digitPos_ > 0 ||
                (currentField_ == Field::RED && r_ > 0) ||
                (currentField_ == Field::GREEN && g_ > 0) ||
                (currentField_ == Field::BLUE && b_ > 0)) {
                clearField();
                return ui::InputResult::CONSUMED;
            }
            return ui::InputResult::REQUEST_POP;

        case ui::KEY_YES:  // Confirm
            clampValues();
            LOG_I(TAG, "RGB confirmed: R=%d G=%d B=%d", r_, g_, b_);
            if (onConfirm_) {
                onConfirm_(r_, g_, b_);
            }
            return ui::InputResult::REQUEST_POP;

        default:
            return ui::InputResult::IGNORED;
    }
}

/**
 * \brief Returns footer hint text for RGB editor.
 * \return Hint string.
 */
const char* RgbInputView::getFooterHint() const {
    return "0-9:Input 4/6:Field Y:OK";
}

/**
 * \brief Renders RGB editor layout and preview box.
 * \param partial `true` for partial redraw, `false` for full redraw.
 */
void RgbInputView::render(bool partial) {
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

    // Title
    if (title_) {
        gfx->setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        gfx->getTextBounds(title_, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((width - w) / 2, TITLE_Y);
        gfx->print(title_);
    }

    // RGB values: "R:255 G:255 B:255"
    char rgbStr[24];
    snprintf(rgbStr, sizeof(rgbStr), "R:%3d  G:%3d  B:%3d", r_, g_, b_);

    gfx->setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(rgbStr, 0, 0, &x1, &y1, &w, &h);
    int startX = (width - w) / 2;
    gfx->setCursor(startX, RGB_Y);
    gfx->print(rgbStr);

    // Clear old underline
    gfx->fillRect(0, UNDERLINE_Y, width, 4, EPD_WHITE);

    // Draw underline for current field
    int charWidth = 12;  // Approximate char width at size 2
    int underlineX = startX;
    int underlineW = 3 * charWidth;

    switch (currentField_) {
        case Field::RED:
            underlineX = startX + 2 * charWidth;  // After "R:"
            break;
        case Field::GREEN:
            underlineX = startX + 9 * charWidth;  // After "R:255  G:"
            break;
        case Field::BLUE:
            underlineX = startX + 16 * charWidth;  // After "R:255  G:255  B:"
            break;
    }

    gfx->fillRect(underlineX, UNDERLINE_Y, underlineW, 3, EPD_BLACK);

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

    // Navigation hint
    gfx->setCursor(10, HINT_Y);
    gfx->print(ui::tr(ui::StringId::HINT_FIELD_NAV));

    // Footer
    const char* hint = getFooterHint();
    if (hint) {
        gfx->fillRect(0, height - 16, width, 16, EPD_BLACK);
        gfx->setTextColor(EPD_WHITE);
        gfx->setCursor(4, height - 12);
        gfx->print(hint);
    }

    dirty_ = false;
}

} // namespace cdc::grove_led
