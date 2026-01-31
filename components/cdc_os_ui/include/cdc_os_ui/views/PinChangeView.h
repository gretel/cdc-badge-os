#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * PinChangeView - PIN change wizard
 *
 * Three-step flow:
 * 1. Enter current PIN (verification)
 * 2. Enter new PIN
 * 3. Confirm new PIN
 *
 * Validates PIN length and calls PinManager on success.
 *
 * Keys:
 *   0-9 = Add digit
 *   N = Backspace/Cancel
 *   Y = Confirm step
 */
class PinChangeView : public ViewBase {
public:
    static constexpr uint8_t MAX_PIN_LENGTH = 16;
    static constexpr uint32_t MESSAGE_DISPLAY_MS = 2000;

    enum class Step : uint8_t {
        CURRENT_PIN,    // Enter current PIN
        NEW_PIN,        // Enter new PIN
        CONFIRM_PIN     // Confirm new PIN
    };

    /**
     * Callback when PIN change is complete
     * @param success true if PIN was changed successfully
     */
    using CompleteCallback = void(*)(bool success);
    using VerifyCallback = bool(*)(const char* pin);
    using ChangeCallback = bool(*)(const char* currentPin, const char* newPin);
    using RetriesCallback = uint8_t(*)();
    using BlockedCallback = bool(*)();

    /**
     * Initialize PIN change view
     * @param minLength Minimum PIN length (default 4)
     * @param maxLength Maximum PIN length (default 16)
     */
    void init(uint8_t minLength = 4, uint8_t maxLength = 16);

    /**
     * Set completion callback
     */
    void setOnComplete(CompleteCallback callback) { onComplete_ = callback; }
    void setVerifyCallback(VerifyCallback callback) { onVerify_ = callback; }
    void setChangeCallback(ChangeCallback callback) { onChange_ = callback; }
    void setRetriesCallback(RetriesCallback callback) { onRetries_ = callback; }
    void setBlockedCallback(BlockedCallback callback) { onBlocked_ = callback; }
    void setTitle(const char* title) { title_ = title; }

    /**
     * Get current step
     */
    Step getStep() const { return step_; }

    /**
     * Get retries remaining (from PinManager)
     */
    uint8_t getRetriesRemaining() const;

    // IView implementation
    void onEnter(void* context) override;
    void render(bool partial) override;
    InputResult onKey(char key) override;
    void onTick(uint32_t nowMs) override;
    const char* getName() const override { return "PinChangeView"; }
    const char* getFooterHint() const override;

private:
    Step step_ = Step::CURRENT_PIN;
    char currentPin_[MAX_PIN_LENGTH + 1] = {};
    char newPin_[MAX_PIN_LENGTH + 1] = {};
    char confirmPin_[MAX_PIN_LENGTH + 1] = {};
    uint8_t length_ = 0;
    uint8_t minLength_ = 4;
    uint8_t maxLength_ = 16;
    const char* message_ = nullptr;
    uint32_t messageShownMs_ = 0;
    bool pinChanged_ = false;

    CompleteCallback onComplete_ = nullptr;
    VerifyCallback onVerify_ = nullptr;
    ChangeCallback onChange_ = nullptr;
    RetriesCallback onRetries_ = nullptr;
    BlockedCallback onBlocked_ = nullptr;
    const char* title_ = nullptr;

    void addDigit(char digit);
    void backspace();
    void confirmStep();
    void showMessage(const char* msg);
    void clearBuffer();
    char* getCurrentBuffer();
    const char* getStepTitle() const;
};

} // namespace cdc::ui
