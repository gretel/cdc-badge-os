/**
 * InfoView Implementation
 *
 * Scrollable text display for help screens, about pages, etc.
 */

#include "cdc_views/InfoView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <cstring>

static const char* TAG = "InfoView";

// Display layout constants
static constexpr int TITLE_Y = 5;
static constexpr int TEXT_START_Y = 28;
static constexpr int TEXT_MARGIN = 8;
static constexpr int FOOTER_HEIGHT = 16;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;

namespace cdc::ui {

void InfoView::init(const char* title, const char* text) {
    // Copy title to internal buffer
    if (title) {
        strncpy(titleBuf_, title, MAX_TITLE_LEN - 1);
        titleBuf_[MAX_TITLE_LEN - 1] = '\0';
    } else {
        titleBuf_[0] = '\0';
    }

    // Copy text to internal buffer
    if (text) {
        strncpy(textBuf_, text, MAX_TEXT_LEN - 1);
        textBuf_[MAX_TEXT_LEN - 1] = '\0';
    } else {
        textBuf_[0] = '\0';
    }

    scrollLine_ = 0;
    totalLines_ = countLines();
    customHint_ = nullptr;
    dirty_ = true;

    LOG_D(TAG, "init: title='%s', lines=%d", titleBuf_, totalLines_);
}

uint16_t InfoView::countLines() const {
    if (textBuf_[0] == '\0') return 0;

    uint16_t lines = 1;
    const char* p = textBuf_;
    while (*p) {
        if (*p == '\n') lines++;
        p++;
    }
    return lines;
}

void InfoView::scroll(bool down) {
    if (totalLines_ <= VISIBLE_LINES) return;

    if (down) {
        if (scrollLine_ < totalLines_ - VISIBLE_LINES) {
            scrollLine_++;
        } else {
            // Wrap to top
            scrollLine_ = 0;
        }
    } else {
        if (scrollLine_ > 0) {
            scrollLine_--;
        } else {
            // Wrap to bottom
            scrollLine_ = totalLines_ - VISIBLE_LINES;
        }
    }

    dirty_ = true;
}

InputResult InfoView::onKey(char key) {
    if (key == 'Y' && onYes_) {
        onYes_(callbackUserData_);
        return InputResult::CONSUMED;
    }
    if (key == 'N' && onNo_) {
        onNo_(callbackUserData_);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case '2': // Up
            scroll(false);
            return InputResult::CONSUMED;

        case '8': // Down
            scroll(true);
            return InputResult::CONSUMED;

        case 'N': // Back
        case 'Y': // Also back (info is read-only)
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}

const char* InfoView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    return tr(StringId::HINT_SCROLL_BACK);
}

void InfoView::render(bool partial) {
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
    gfx->setTextSize(1);

    // Title
    gfx->setCursor(TEXT_MARGIN, TITLE_Y);
    if (titleBuf_[0] != '\0') {
        gfx->print(titleBuf_);
    }
    gfx->drawFastHLine(0, TITLE_Y + 18, width, EPD_BLACK);

    // Text area dimensions
    int textAreaWidth = width - TEXT_MARGIN * 2 - SCROLL_INDICATOR_WIDTH;
    int textAreaHeight = height - TEXT_START_Y - FOOTER_HEIGHT;

    // Clear text area
    gfx->fillRect(TEXT_MARGIN, TEXT_START_Y, textAreaWidth, textAreaHeight, EPD_WHITE);

    // Render visible lines
    if (textBuf_[0] != '\0') {
        const char* lineStart = textBuf_;
        uint16_t currentLine = 0;
        int y = TEXT_START_Y;

        // Skip to scroll position
        while (currentLine < scrollLine_ && *lineStart) {
            if (*lineStart == '\n') currentLine++;
            lineStart++;
        }

        // Render visible lines
        for (uint8_t i = 0; i < VISIBLE_LINES && *lineStart; i++) {
            gfx->setCursor(TEXT_MARGIN, y);

            // Find end of line
            const char* lineEnd = lineStart;
            while (*lineEnd && *lineEnd != '\n') lineEnd++;

            // Print line (character by character to handle no null-terminator)
            while (lineStart < lineEnd) {
                gfx->print(*lineStart);
                lineStart++;
            }

            // Skip newline
            if (*lineStart == '\n') lineStart++;

            y += LINE_HEIGHT;
        }
    }

    // Scroll indicators (if needed)
    if (totalLines_ > VISIBLE_LINES) {
        int indicatorX = width - SCROLL_INDICATOR_WIDTH;
        int listHeight = VISIBLE_LINES * LINE_HEIGHT;

        gfx->fillRect(indicatorX, TEXT_START_Y, SCROLL_INDICATOR_WIDTH, listHeight, EPD_WHITE);

        // Up arrow
        if (scrollLine_ > 0) {
            gfx->fillTriangle(
                indicatorX + 4, TEXT_START_Y + 4,
                indicatorX + 1, TEXT_START_Y + 10,
                indicatorX + 7, TEXT_START_Y + 10,
                EPD_BLACK
            );
        }

        // Down arrow
        if (scrollLine_ + VISIBLE_LINES < totalLines_) {
            int arrowY = TEXT_START_Y + listHeight - 12;
            gfx->fillTriangle(
                indicatorX + 4, arrowY + 8,
                indicatorX + 1, arrowY + 2,
                indicatorX + 7, arrowY + 2,
                EPD_BLACK
            );
        }

        // Scroll bar
        int barHeight = listHeight - 24;
        int thumbHeight = barHeight * VISIBLE_LINES / totalLines_;
        if (thumbHeight < 10) thumbHeight = 10;
        int scrollRange = totalLines_ - VISIBLE_LINES;
        int thumbPos = scrollRange > 0 ? (barHeight - thumbHeight) * scrollLine_ / scrollRange : 0;

        gfx->drawRect(indicatorX + 2, TEXT_START_Y + 12, 4, barHeight, EPD_BLACK);
        gfx->fillRect(indicatorX + 2, TEXT_START_Y + 12 + thumbPos, 4, thumbHeight, EPD_BLACK);
    }

    // Footer
    gfx->fillRect(0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
    gfx->setCursor(4, height - 12);

    // Line counter
    if (totalLines_ > VISIBLE_LINES) {
        char posStr[20];  // Max: "65535-65535/65535  \0"
        snprintf(posStr, sizeof(posStr), "%u-%u/%u  ",
                 scrollLine_ + 1,
                 scrollLine_ + VISIBLE_LINES > totalLines_ ? totalLines_ : scrollLine_ + VISIBLE_LINES,
                 totalLines_);
        gfx->print(posStr);
    }

    const char* hint = getFooterHint();
    if (hint) {
        gfx->print(hint);
    }

    dirty_ = false;
}

// ============================================================================
// Convenience Function
// ============================================================================

static InfoView s_sharedInfoView;

InfoView* showInfo(const char* title, const char* text, const char* hint) {
    s_sharedInfoView.init(title, text);
    if (hint) {
        s_sharedInfoView.setHint(hint);
    }
    ViewStack::instance().push(&s_sharedInfoView);
    return &s_sharedInfoView;
}

} // namespace cdc::ui
