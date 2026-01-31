/**
 * ConfirmView Implementation
 *
 * Y/N confirmation dialog
 */

#include "cdc_views/ConfirmView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_hal/IDisplay.h"
#include <goodisplay/gdey029T94.h>
#include <cstring>

namespace cdc::ui {

void ConfirmView::init(const char* message, Icon icon) {
    if (message) {
        strncpy(message_, message, MAX_MSG_LEN - 1);
        message_[MAX_MSG_LEN - 1] = '\0';
    } else {
        message_[0] = '\0';
    }
    icon_ = icon;
    dirty_ = true;
}

InputResult ConfirmView::onKey(char key) {
    if (key == 'Y') {
        ViewStack::instance().hideModal();
        if (onConfirm_) {
            onConfirm_(confirmUserData_);
        }
        return InputResult::CONSUMED;
    }

    if (key == 'N') {
        ViewStack::instance().hideModal();
        if (onCancel_) {
            onCancel_(cancelUserData_);
        }
        return InputResult::CONSUMED;
    }

    return InputResult::IGNORED;
}

void ConfirmView::render(bool partial) {
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
    int textY = boxY + 20;

    // Draw icon if present
    if (icon_ != Icon::NONE) {
        int iconX = boxX + 20;
        int iconY = boxY + 22;

        switch (icon_) {
            case Icon::QUESTION:
                // Question mark in circle
                gfx->drawCircle(iconX, iconY, 8, EPD_BLACK);
                gfx->setCursor(iconX - 3, iconY + 4);
                gfx->print("?");
                break;

            case Icon::WARNING:
                // Warning triangle
                gfx->drawTriangle(iconX, iconY - 7, iconX - 7, iconY + 6, iconX + 7, iconY + 6, EPD_BLACK);
                gfx->fillRect(iconX - 1, iconY - 2, 2, 5, EPD_BLACK);
                gfx->fillRect(iconX - 1, iconY + 4, 2, 2, EPD_BLACK);
                break;

            case Icon::ERROR:
                // X in circle
                gfx->drawCircle(iconX, iconY, 8, EPD_BLACK);
                gfx->drawLine(iconX - 4, iconY - 4, iconX + 4, iconY + 4, EPD_BLACK);
                gfx->drawLine(iconX - 4, iconY + 4, iconX + 4, iconY - 4, EPD_BLACK);
                break;

            default:
                break;
        }

        textX = boxX + 40;  // Shift text right when icon present
    }

    // Draw message text (possibly multi-line for long messages)
    gfx->setCursor(textX, textY);

    // Simple word wrap for longer messages
    const char* ptr = message_;
    int lineY = textY;
    int maxLineWidth = BOX_WIDTH - (textX - boxX) - 10;
    char lineBuf[48];
    int lineLen = 0;

    while (*ptr) {
        // Find next word boundary
        const char* wordEnd = ptr;
        while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n') wordEnd++;

        int wordLen = wordEnd - ptr;

        // Check if word fits on current line
        if (lineLen + wordLen + 1 < (int)sizeof(lineBuf) - 1 &&
            (lineLen + wordLen) * 6 < maxLineWidth) {
            // Add space if not first word
            if (lineLen > 0) {
                lineBuf[lineLen++] = ' ';
            }
            memcpy(lineBuf + lineLen, ptr, wordLen);
            lineLen += wordLen;
        } else {
            // Print current line and start new one
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                gfx->setCursor(textX, lineY);
                gfx->print(lineBuf);
                lineY += 12;
                lineLen = 0;
            }
            // Start new line with current word
            memcpy(lineBuf, ptr, wordLen);
            lineLen = wordLen;
        }

        ptr = wordEnd;
        while (*ptr == ' ') ptr++;
        if (*ptr == '\n') {
            // Force line break
            lineBuf[lineLen] = '\0';
            gfx->setCursor(textX, lineY);
            gfx->print(lineBuf);
            lineY += 12;
            lineLen = 0;
            ptr++;
        }
    }

    // Print remaining text
    if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        gfx->setCursor(textX, lineY);
        gfx->print(lineBuf);
    }

    // Draw Y/N hint at bottom
    gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
    gfx->print("Y=Ja  N=Nein");

    dirty_ = false;
}

// ============================================================================
// Convenience Functions
// ============================================================================

static ConfirmView s_sharedConfirm;

void showConfirm(const char* message,
                 ConfirmView::ConfirmCallback onConfirm,
                 ConfirmView::CancelCallback onCancel,
                 ConfirmView::Icon icon,
                 void* userData) {
    s_sharedConfirm.init(message, icon);
    s_sharedConfirm.setOnConfirm(onConfirm, userData);
    s_sharedConfirm.setOnCancel(onCancel, userData);
    ViewStack::instance().showModal(&s_sharedConfirm);
    ViewStack::instance().render();
}

} // namespace cdc::ui
