#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * MessageBox Icon Types
 */
enum class MessageIcon : uint8_t {
    NONE = 0,       // No icon
    SUCCESS,        // Checkmark
    ERROR,          // X mark
    INFO,           // Info circle
    WARNING         // Warning triangle
};

/**
 * MessageBox - System feedback overlay
 *
 * Displays a centered message box for user feedback.
 * Can have optional icon and auto-dismiss timeout.
 *
 * Keys:
 *   Y/N = Close (if interactive)
 */
class MessageBox : public ViewBase {
public:
    /**
     * Close callback (called when message is dismissed)
     */
    using CloseCallback = void(*)();

    /**
     * Initialize message box
     * @param message Text to display
     * @param icon Icon type (NONE, SUCCESS, ERROR, INFO, WARNING)
     * @param timeoutMs Auto-dismiss timeout (0 = no auto-dismiss)
     */
    void init(const char* message, MessageIcon icon = MessageIcon::NONE,
              uint32_t timeoutMs = 0);

    /**
     * Set close callback
     */
    void setOnClose(CloseCallback callback) { onClose_ = callback; }

    // IView implementation
    void render(bool partial) override;
    InputResult onKey(char key) override;
    void onTick(uint32_t nowMs) override;
    const char* getName() const override { return "MessageBox"; }
    const char* getFooterHint() const override { return nullptr; }

private:
    const char* message_ = nullptr;
    MessageIcon icon_ = MessageIcon::NONE;
    uint32_t timeoutMs_ = 0;
    uint32_t startTimeMs_ = 0;
    CloseCallback onClose_ = nullptr;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Show a message box overlay (modal).
 * Simplest API for user feedback.
 *
 * @param message Text to display
 * @param icon Icon type
 * @param timeoutMs Auto-dismiss timeout (0 = manual dismiss with Y/N)
 * @param onClose Optional callback when dismissed
 *
 * Examples:
 *   showMessage("Saved!");                              // Simple info
 *   showMessage("Success!", MessageIcon::SUCCESS, 2000); // Auto-dismiss after 2s
 *   showMessage("Wrong PIN!", MessageIcon::ERROR, 1500); // Error with timeout
 */
void showMessage(const char* message, MessageIcon icon = MessageIcon::NONE,
                 uint32_t timeoutMs = 0, MessageBox::CloseCallback onClose = nullptr);

/**
 * Shorthand for success message
 */
inline void showSuccess(const char* message, uint32_t timeoutMs = 2000) {
    showMessage(message, MessageIcon::SUCCESS, timeoutMs);
}

/**
 * Shorthand for error message
 */
inline void showError(const char* message, uint32_t timeoutMs = 2000) {
    showMessage(message, MessageIcon::ERROR, timeoutMs);
}

/**
 * Hide current message box (if visible)
 */
void hideMessage();

} // namespace cdc::ui
