/**
 * PinEntryView Implementation
 *
 * Secure PIN code input with masked display.
 * Errors are shown via MessageBox overlay.
 */

#include "cdc_views/PinEntryView.h"
#include "cdc_views/MessageBox.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_core/PinManager.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <cstring>

static const char* TAG = "PinEntryView";

// Display constants
static constexpr int TITLE_Y = 15;
static constexpr int PIN_Y = 50;
static constexpr int PIN_DOT_SIZE = 16;
static constexpr int PIN_DOT_SPACING = 24;
static constexpr int RETRIES_Y = 80;

namespace cdc::ui {

void PinEntryView::init(const char* title, uint8_t maxPinLength, uint8_t maxAttempts) {
    title_ = title;
    maxLength_ = maxPinLength > MAX_PIN_LENGTH ? MAX_PIN_LENGTH : maxPinLength;
    maxAttempts_ = maxAttempts;
    minLength_ = 4;  // Default minimum
    clear();
    dirty_ = true;
}

void PinEntryView::onEnter(void* context) {
    (void)context;
    clear();
    dirty_ = true;
}

void PinEntryView::clear() {
    memset(buffer_, 0, sizeof(buffer_));
    length_ = 0;
}

void PinEntryView::addDigit(char digit) {
    if (length_ >= maxLength_ || lockedOut_) return;

    buffer_[length_++] = digit;
    buffer_[length_] = '\0';
    dirty_ = true;
}

void PinEntryView::backspace() {
    if (length_ > 0 && !lockedOut_) {
        buffer_[--length_] = '\0';
        dirty_ = true;
    }
}

void PinEntryView::onTick(uint32_t nowMs) {
    (void)nowMs;
    core::PinManager& pm = core::PinManager::instance();

    // Check if blocked status changed
    bool blocked = pm.isBadgeBlocked();
    if (blocked != lockedOut_) {
        lockedOut_ = blocked;
        dirty_ = true;
    }

    // Update display every second during lockout to show countdown
    if (lockedOut_) {
        static uint32_t lastUpdate = 0;
        if (nowMs - lastUpdate >= 1000) {
            lastUpdate = nowMs;
            dirty_ = true;
        }
    }
}

uint32_t PinEntryView::getLockoutRemaining() const {
    return core::PinManager::instance().getLockoutRemainingMs();
}

void PinEntryView::verify() {
    if (length_ < minLength_) {
        if (showMessages_) {
            showMessage(tr(StringId::PIN_TOO_SHORT), MessageIcon::WARNING, 1500);
        }
        return;
    }

    if (!onVerify_) {
        // No verify callback, just accept
        if (onSuccess_) {
            onSuccess_();
        }
        return;
    }

    bool valid = onVerify_(buffer_);
    if (valid) {
        LOG_I(TAG, "PIN verified successfully");
        if (onSuccess_) {
            onSuccess_();
        }
    } else {
        attempts_++;
        LOG_W(TAG, "PIN verification failed, attempt %d", attempts_);

        // Check PinManager for block status (it manages retries persistently)
        core::PinManager& pm = core::PinManager::instance();
        if (pm.isBadgeBlocked()) {
            lockedOut_ = true;
            if (showMessages_) {
                showMessage(tr(StringId::LOCKED_OUT), MessageIcon::ERROR, 3000);
            }
        } else {
            if (showMessages_) {
                showMessage(tr(StringId::WRONG_PIN), MessageIcon::ERROR, 1500);
            }
        }
        clear();
        dirty_ = true;
        if (onFailure_) {
            onFailure_(lockedOut_);
        }
    }
}

InputResult PinEntryView::onKey(char key) {
    if (lockedOut_) {
        return InputResult::IGNORED;
    }

    // Handle digits
    if (key >= '0' && key <= '9') {
        addDigit(key);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case 'N': // Backspace or cancel
            if (length_ > 0) {
                backspace();
            } else {
                if (onCancel_) {
                    onCancel_();
                    return InputResult::CONSUMED;
                }
                return InputResult::REQUEST_POP;
            }
            return InputResult::CONSUMED;

        case 'Y': // Confirm
            verify();
            return InputResult::CONSUMED;

        default:
            return InputResult::IGNORED;
    }
}

const char* PinEntryView::getFooterHint() const {
    return tr(StringId::HINT_PIN_INPUT);
}

void PinEntryView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) {
        LOG_E(TAG, "display is null!");
        return;
    }

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) {
        LOG_E(TAG, "gfx handle is null!");
        return;
    }

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

    // PIN dots (centered)
    int totalWidth = maxLength_ * PIN_DOT_SIZE + (maxLength_ - 1) * (PIN_DOT_SPACING - PIN_DOT_SIZE);
    int startX = (width - totalWidth) / 2;

    for (uint8_t i = 0; i < maxLength_; i++) {
        int x = startX + i * PIN_DOT_SPACING;
        int y = PIN_Y;

        if (i < length_) {
            // Filled dot for entered digits
            gfx->fillCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
        } else {
            // Empty dot for remaining positions
            gfx->drawCircle(x + PIN_DOT_SIZE / 2, y + PIN_DOT_SIZE / 2, PIN_DOT_SIZE / 2 - 1, EPD_BLACK);
        }
    }

    // Show remaining retries or blocked status with countdown
    {
        core::PinManager& pm = core::PinManager::instance();
        gfx->setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        char statusStr[32];

        if (pm.isBadgeBlocked()) {
            uint32_t remainingMs = pm.getLockoutRemainingMs();
            uint32_t remainingSec = (remainingMs + 999) / 1000;  // Round up
            snprintf(statusStr, sizeof(statusStr), "%s: %lus", tr(StringId::LOCKED_OUT), remainingSec);
        } else {
            uint8_t retries = pm.getBadgeRetries();
            snprintf(statusStr, sizeof(statusStr), "%s: %d", tr(StringId::RETRIES), retries);
        }

        gfx->getTextBounds(statusStr, 0, 0, &x1, &y1, &w, &h);
        gfx->setCursor((width - w) / 2, RETRIES_Y);
        gfx->print(statusStr);
    }

    // Footer hint
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

static PinEntryView s_sharedPinEntry;

PinEntryView* showPinEntry(const char* title,
                           PinEntryView::VerifyCallback onVerify,
                           PinEntryView::SuccessCallback onSuccess,
                           uint8_t maxLength,
                           uint8_t minLength,
                           uint8_t maxAttempts) {
    s_sharedPinEntry.init(title, maxLength, maxAttempts);
    s_sharedPinEntry.setMinLength(minLength);
    s_sharedPinEntry.setOnVerify(onVerify);
    s_sharedPinEntry.setOnSuccess(onSuccess);
    ViewStack::instance().push(&s_sharedPinEntry);
    return &s_sharedPinEntry;
}

} // namespace cdc::ui
