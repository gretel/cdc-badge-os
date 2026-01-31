#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::grove_led {

/**
 * RgbInputView - RGB color input with R/G/B fields (0-255 each)
 *
 * Navigation:
 *   0-9 = Enter digits
 *   4 = Previous field
 *   6 = Next field
 *   N = Clear current field / Cancel (if empty)
 *   Y = Confirm
 */
class RgbInputView : public ui::ViewBase {
public:
    /**
     * Confirm callback
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    using ConfirmCallback = void(*)(uint8_t r, uint8_t g, uint8_t b);

    /**
     * Initialize RGB input view
     * @param title View title
     * @param r Initial red value (0-255)
     * @param g Initial green value (0-255)
     * @param b Initial blue value (0-255)
     */
    void init(const char* title, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Set confirm callback
     */
    void setOnConfirm(ConfirmCallback callback) { onConfirm_ = callback; }

    /**
     * Get current values
     */
    uint8_t getR() const { return r_; }
    uint8_t getG() const { return g_; }
    uint8_t getB() const { return b_; }

    // IView implementation
    void render(bool partial) override;
    ui::InputResult onKey(char key) override;
    const char* getName() const override { return "RgbInputView"; }
    const char* getFooterHint() const override;

private:
    enum class Field : uint8_t { RED = 0, GREEN = 1, BLUE = 2 };

    const char* title_ = nullptr;
    uint8_t r_ = 255;
    uint8_t g_ = 255;
    uint8_t b_ = 255;
    Field currentField_ = Field::RED;
    uint8_t digitPos_ = 0;  // Position within current field (0-2 for 3 digits)
    ConfirmCallback onConfirm_ = nullptr;

    void nextField();
    void prevField();
    void clearField();
    void enterDigit(char digit);
    void clampValues();
};

} // namespace cdc::grove_led
