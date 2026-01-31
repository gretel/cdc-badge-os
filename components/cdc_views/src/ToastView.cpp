/**
 * ToastView Implementation
 *
 * Temporary overlay message with auto-dismiss
 */

#include "cdc_views/ToastView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_hal/IDisplay.h"
#include <goodisplay/gdey029T94.h>
#include <cstring>
#include "esp_timer.h"

namespace cdc::ui {

void ToastView::init(const char* message, Icon icon, uint16_t durationMs, bool dismissible) {
    if (message) {
        strncpy(message_, message, MAX_MSG_LEN - 1);
        message_[MAX_MSG_LEN - 1] = '\0';
    } else {
        message_[0] = '\0';
    }

    icon_ = icon;
    durationMs_ = durationMs;
    dismissible_ = dismissible;
    startMs_ = esp_timer_get_time() / 1000;
    expired_ = false;
    dirty_ = true;
}

void ToastView::onTick(uint32_t nowMs) {
    if (durationMs_ > 0 && !expired_) {
        if (nowMs - startMs_ >= durationMs_) {
            expired_ = true;
            ViewStack::instance().hideModal();
        }
    }
}

InputResult ToastView::onKey(char key) {
    // Any Y or N key dismisses the toast (if dismissible)
    if (dismissible_ && (key == 'Y' || key == 'N')) {
        expired_ = true;
        ViewStack::instance().hideModal();
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}

void ToastView::render(bool partial) {
    (void)partial;

    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    const uint16_t width = display->getWidth();
    const uint16_t height = display->getHeight();

    // Calculate centered position
    int boxX = (width - BOX_WIDTH) / 2;
    int boxY = (height - BOX_HEIGHT) / 2;

    // Draw white box with double black border
    gfx->fillRect(boxX, boxY, BOX_WIDTH, BOX_HEIGHT, EPD_WHITE);
    gfx->drawRect(boxX, boxY, BOX_WIDTH, BOX_HEIGHT, EPD_BLACK);
    gfx->drawRect(boxX + 1, boxY + 1, BOX_WIDTH - 2, BOX_HEIGHT - 2, EPD_BLACK);

    gfx->setTextColor(EPD_BLACK);
    gfx->setTextSize(1);

    // Text position (adjusted if icon present)
    int textX = boxX + 15;
    int textY = boxY + (BOX_HEIGHT / 2) + 4;

    // Draw icon if present
    if (icon_ != Icon::NONE) {
        int iconX = boxX + 20;
        int iconY = boxY + (BOX_HEIGHT / 2);

        switch (icon_) {
            case Icon::SUCCESS:
                // Checkmark
                gfx->drawLine(iconX - 5, iconY, iconX - 2, iconY + 4, EPD_BLACK);
                gfx->drawLine(iconX - 2, iconY + 4, iconX + 6, iconY - 5, EPD_BLACK);
                // Thicker
                gfx->drawLine(iconX - 5, iconY + 1, iconX - 2, iconY + 5, EPD_BLACK);
                gfx->drawLine(iconX - 2, iconY + 5, iconX + 6, iconY - 4, EPD_BLACK);
                break;

            case Icon::ERROR:
                // X mark
                gfx->drawLine(iconX - 5, iconY - 5, iconX + 5, iconY + 5, EPD_BLACK);
                gfx->drawLine(iconX - 5, iconY + 5, iconX + 5, iconY - 5, EPD_BLACK);
                // Thicker
                gfx->drawLine(iconX - 4, iconY - 5, iconX + 6, iconY + 5, EPD_BLACK);
                gfx->drawLine(iconX - 4, iconY + 5, iconX + 6, iconY - 5, EPD_BLACK);
                break;

            case Icon::INFO:
                // Circle with i
                gfx->drawCircle(iconX, iconY, 6, EPD_BLACK);
                gfx->fillRect(iconX - 1, iconY - 3, 2, 2, EPD_BLACK);  // Dot
                gfx->fillRect(iconX - 1, iconY, 2, 5, EPD_BLACK);       // Stem
                break;
            case Icon::TASK:
                // Simple hourglass icon
                gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY - 6, EPD_BLACK);
                gfx->drawLine(iconX - 5, iconY + 6, iconX + 5, iconY + 6, EPD_BLACK);
                gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY + 6, EPD_BLACK);
                gfx->drawLine(iconX + 5, iconY - 6, iconX - 5, iconY + 6, EPD_BLACK);
                gfx->fillTriangle(iconX - 3, iconY - 4, iconX + 3, iconY - 4, iconX, iconY - 1, EPD_BLACK);
                gfx->fillTriangle(iconX - 3, iconY + 4, iconX + 3, iconY + 4, iconX, iconY + 1, EPD_BLACK);
                break;
            case Icon::ALERT:
                // Warning triangle with exclamation
                gfx->drawTriangle(iconX, iconY - 7, iconX - 6, iconY + 6, iconX + 6, iconY + 6, EPD_BLACK);
                gfx->fillRect(iconX - 1, iconY - 2, 2, 5, EPD_BLACK);
                gfx->fillRect(iconX - 1, iconY + 4, 2, 2, EPD_BLACK);
                break;

            default:
                break;
        }

        textX = boxX + 40;  // Shift text right when icon present
    }

    // Draw message text
    gfx->setCursor(textX, textY);
    gfx->print(message_);

    dirty_ = false;
}

// ============================================================================
// Convenience Functions
// ============================================================================

static ToastView s_sharedToast;

static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible = true) {
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    ViewStack::instance().render();  // Immediate render for toast
}

void showToast(const char* message, uint16_t durationMs) {
    showToastInternal(message, ToastView::Icon::NONE, durationMs);
}

void showToastSuccess(const char* message, uint16_t durationMs) {
    showToastInternal(message, ToastView::Icon::SUCCESS, durationMs);
}

void showToastError(const char* message, uint16_t durationMs) {
    showToastInternal(message, ToastView::Icon::ERROR, durationMs);
}

void showToastInfo(const char* message, uint16_t durationMs) {
    showToastInternal(message, ToastView::Icon::INFO, durationMs);
}

void showToastTask(const char* message, uint16_t durationMs) {
    showToastInternal(message, ToastView::Icon::TASK, durationMs);
}

void showToastAlert(const char* message, uint16_t durationMs) {
    showToastInternal(message, ToastView::Icon::ALERT, durationMs);
}

void showToastAlertSticky(const char* message) {
    showToastInternal(message, ToastView::Icon::ALERT, 0, false);
}

} // namespace cdc::ui
