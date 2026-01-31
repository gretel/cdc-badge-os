#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * ConfirmView - Y/N confirmation dialog
 *
 * Shows a message with Y (confirm) and N (cancel) options.
 * Useful for "Are you sure?" type dialogs.
 *
 * Keys:
 *   Y = Confirm (triggers onConfirm callback)
 *   N = Cancel (triggers onCancel callback or pops view)
 */
class ConfirmView : public ViewBase {
public:
    enum class Icon : uint8_t {
        NONE = 0,
        QUESTION,
        WARNING,
        ERROR
    };

    using ConfirmCallback = void(*)(void* userData);
    using CancelCallback = void(*)(void* userData);

    /**
     * Initialize confirm dialog
     * @param message Question/message to display
     * @param icon Optional icon
     */
    void init(const char* message, Icon icon = Icon::QUESTION);

    /**
     * Set confirm (Y) callback
     */
    void setOnConfirm(ConfirmCallback callback, void* userData = nullptr) {
        onConfirm_ = callback;
        confirmUserData_ = userData;
    }

    /**
     * Set cancel (N) callback (optional - defaults to just closing)
     */
    void setOnCancel(CancelCallback callback, void* userData = nullptr) {
        onCancel_ = callback;
        cancelUserData_ = userData;
    }

    // IView implementation
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "ConfirmView"; }
    const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }

private:
    static constexpr uint16_t MAX_MSG_LEN = 96;
    static constexpr int BOX_WIDTH = 220;
    static constexpr int BOX_HEIGHT = 60;

    char message_[MAX_MSG_LEN] = {};
    Icon icon_ = Icon::QUESTION;
    ConfirmCallback onConfirm_ = nullptr;
    CancelCallback onCancel_ = nullptr;
    void* confirmUserData_ = nullptr;
    void* cancelUserData_ = nullptr;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Show a confirmation dialog (modal)
 * @param message Question to display
 * @param onConfirm Callback when Y is pressed
 * @param onCancel Callback when N is pressed (optional)
 * @param icon Icon type
 * @param userData User data passed to callbacks
 */
void showConfirm(const char* message,
                 ConfirmView::ConfirmCallback onConfirm,
                 ConfirmView::CancelCallback onCancel = nullptr,
                 ConfirmView::Icon icon = ConfirmView::Icon::QUESTION,
                 void* userData = nullptr);

/**
 * Show a simple Y/N confirm and return result via callback
 * Shorthand for common "Are you sure?" pattern
 */
inline void askConfirm(const char* message, ConfirmView::ConfirmCallback onYes, void* userData = nullptr) {
    showConfirm(message, onYes, nullptr, ConfirmView::Icon::QUESTION, userData);
}

} // namespace cdc::ui
