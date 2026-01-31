#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * TimeInputView - Time input with hour/minute fields
 *
 * Navigation:
 *   0-9 = Enter digits
 *   4 = Previous field
 *   6 = Next field
 *   N = Clear current field / Cancel (if empty)
 *   Y = Confirm
 */
class TimeInputView : public ViewBase {
public:
    /**
     * Confirm callback
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     */
    using ConfirmCallback = void(*)(uint8_t hour, uint8_t minute);

    /**
     * Initialize time input view
     * @param title View title
     * @param hour Initial hour (0-23)
     * @param minute Initial minute (0-59)
     */
    void init(const char* title, uint8_t hour, uint8_t minute);

    /**
     * Set confirm callback
     */
    void setOnConfirm(ConfirmCallback callback) { onConfirm_ = callback; }

    /**
     * Get current values
     */
    uint8_t getHour() const { return hour_; }
    uint8_t getMinute() const { return minute_; }

    // IView implementation
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "TimeInputView"; }
    const char* getFooterHint() const override;

private:
    enum class Field : uint8_t { HOUR = 0, MINUTE = 1 };

    const char* title_ = nullptr;
    uint8_t hour_ = 0;
    uint8_t minute_ = 0;
    Field currentField_ = Field::HOUR;
    uint8_t digitPos_ = 0;  // Position within current field
    ConfirmCallback onConfirm_ = nullptr;

    void nextField();
    void prevField();
    void clearField();
    void enterDigit(char digit);
};

} // namespace cdc::ui
