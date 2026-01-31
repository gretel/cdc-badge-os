/**
 * T9InputView Implementation
 *
 * Multi-tap text input like classic phones.
 * Based on legacy views.cpp implementation.
 */

#include "cdc_views/T9InputView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include "esp_timer.h"
#include <goodisplay/gdey029T94.h>
#include <cstring>

static const char* TAG = "T9InputView";

// T9 Character Mappings
static const char* t9_chars[] = {
    " 0",                           // 0 - space, 0
    ".?!,;:'\"()-_@#$%&*+=/\\1",   // 1 - symbols, 1
    "abc2ABC",                      // 2
    "def3DEF",                      // 3
    "ghi4GHI",                      // 4
    "jkl5JKL",                      // 5
    "mno6MNO",                      // 6
    "pqrs7PQRS",                    // 7
    "tuv8TUV",                      // 8
    "wxyz9WXYZ"                     // 9
};

// Display layout constants
static constexpr int TITLE_Y = 5;
static constexpr int TEXT_Y = 50;
static constexpr int FOOTER_HEIGHT = 16;
static constexpr int TEXT_MARGIN = 10;

namespace cdc::ui {

void T9InputView::init(const char* title, const char* initialText, uint16_t maxLen) {
    title_ = title;
    maxLen_ = maxLen > MAX_TEXT_LEN ? MAX_TEXT_LEN : maxLen;

    // Copy initial text
    if (initialText) {
        strncpy(text_, initialText, maxLen_);
        text_[maxLen_] = '\0';
        len_ = strlen(text_);
    } else {
        text_[0] = '\0';
        len_ = 0;
    }

    // Reset T9 state
    lastKey_ = 0;
    charIndex_ = 0;
    lastPressMs_ = 0;
    cursorActive_ = false;
    onSave_ = nullptr;
    dirty_ = true;

    LOG_D(TAG, "init: title='%s', maxLen=%d", title ? title : "(null)", maxLen_);
}

char T9InputView::getChar(char key, uint8_t index) {
    if (key < '0' || key > '9') return '\0';
    const char* chars = t9_chars[key - '0'];
    uint8_t count = strlen(chars);
    return chars[index % count];
}

uint8_t T9InputView::getCharCount(char key) {
    if (key < '0' || key > '9') return 0;
    return strlen(t9_chars[key - '0']);
}

bool T9InputView::processKey(char key) {
    if (key < '0' || key > '9') return false;

    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    bool sameKey = (key == lastKey_);
    bool timeout = (now - lastPressMs_) > TIMEOUT_MS;

    if (sameKey && !timeout && len_ > 0) {
        // Cycle through characters for the same key
        charIndex_++;
        uint8_t charCount = getCharCount(key);
        if (charIndex_ >= charCount) {
            charIndex_ = 0;
        }
        // Replace last character
        text_[len_ - 1] = getChar(key, charIndex_);
        cursorActive_ = true;
    } else {
        // New key or timeout - commit previous and add new
        if (len_ < maxLen_) {
            text_[len_++] = getChar(key, 0);
            text_[len_] = '\0';
            charIndex_ = 0;
            cursorActive_ = true;
        }
    }

    lastKey_ = key;
    lastPressMs_ = now;
    dirty_ = true;

    return true;
}

void T9InputView::backspace() {
    if (len_ > 0) {
        len_--;
        text_[len_] = '\0';
        lastKey_ = 0;
        cursorActive_ = false;
        dirty_ = true;
        LOG_D(TAG, "backspace: text='%s'", text_);
    }
}

void T9InputView::forceDigit(char key) {
    if (key < '0' || key > '9') return;

    // Commit any pending character first
    commitCharacter();

    if (len_ < maxLen_) {
        text_[len_++] = key;
        text_[len_] = '\0';
        dirty_ = true;
        LOG_D(TAG, "forceDigit: key='%c', text='%s'", key, text_);
    }
}

void T9InputView::commitCharacter() {
    if (lastKey_ != 0) {
        lastKey_ = 0;
        cursorActive_ = false;
        dirty_ = true;
    }
}

void T9InputView::onTick(uint32_t nowMs) {
    (void)nowMs;  // Use own timestamp for consistent timing

    // Check for timeout to commit character
    if (lastKey_ != 0) {
        uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        // Safe comparison: only timeout if now > lastPressMs_ (avoid unsigned wrap)
        if (now >= lastPressMs_ && (now - lastPressMs_) > TIMEOUT_MS) {
            commitCharacter();
        }
    }
}

InputResult T9InputView::onKey(char key) {
    switch (key) {
        case 'Y':  // Confirm
            commitCharacter();
            if (onSave_) {
                // Pop ourselves FIRST, then call callback
                // This prevents the callback's pushed view from being popped
                ViewStack::instance().pop();
                onSave_(text_);
            }
            return InputResult::CONSUMED;  // Already popped ourselves

        case 'N':  // Backspace
            backspace();
            return InputResult::CONSUMED;

        default:
            if (key >= '0' && key <= '9') {
                processKey(key);
                return InputResult::CONSUMED;
            }
            return InputResult::IGNORED;
    }
}

InputResult T9InputView::onLongPress(char key) {
    if (key == 'N') {
        // Clear all text
        text_[0] = '\0';
        len_ = 0;
        lastKey_ = 0;
        cursorActive_ = false;
        dirty_ = true;
        return InputResult::CONSUMED;
    }

    if (key >= '0' && key <= '9') {
        // Force insert digit
        forceDigit(key);
        return InputResult::CONSUMED;
    }

    return InputResult::IGNORED;
}

const char* T9InputView::getFooterHint() const {
    return tr(StringId::HINT_T9_INPUT);
}

void T9InputView::render(bool partial) {
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
    if (title_) {
        gfx->print(title_);
    }
    gfx->drawFastHLine(0, TITLE_Y + 18, width, EPD_BLACK);

    // Text input area
    gfx->fillRect(TEXT_MARGIN, TEXT_Y - 5, width - TEXT_MARGIN * 2, 30, EPD_WHITE);
    gfx->drawRect(TEXT_MARGIN - 2, TEXT_Y - 7, width - TEXT_MARGIN * 2 + 4, 34, EPD_BLACK);

    gfx->setCursor(TEXT_MARGIN + 2, TEXT_Y);

    if (len_ == 0 && placeholder_) {
        // Show placeholder when empty
        gfx->setTextColor(EPD_DARKGREY);
        gfx->print(placeholder_);
        gfx->setTextColor(EPD_BLACK);
    } else {
        // Show text with cursor
        for (uint16_t i = 0; i < len_; i++) {
            // If this is the last char and cursor is active, invert it
            if (cursorActive_ && i == len_ - 1) {
                int16_t x = gfx->getCursorX();
                int16_t y = gfx->getCursorY();
                gfx->fillRect(x, y - 2, 8, 14, EPD_BLACK);
                gfx->setTextColor(EPD_WHITE);
                gfx->print(text_[i]);
                gfx->setTextColor(EPD_BLACK);
            } else {
                gfx->print(text_[i]);
            }
        }

        // Show cursor at end if not in T9 cycle
        if (!cursorActive_) {
            gfx->print("|");
        }
    }

    // Footer with hint
    gfx->fillRect(0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
    gfx->setCursor(4, height - 12);

    // Show character count
    char countStr[16];
    snprintf(countStr, sizeof(countStr), "%u/%u  ", len_, maxLen_);
    gfx->print(countStr);

    const char* hint = getFooterHint();
    if (hint) {
        gfx->print(hint);
    }

    dirty_ = false;
}

// ============================================================================
// Convenience Function
// ============================================================================

static T9InputView s_sharedT9Input;

T9InputView* showT9Input(const char* title, const char* initialText,
                         T9InputView::SaveCallback onSave, uint16_t maxLen) {
    s_sharedT9Input.init(title, initialText, maxLen);
    s_sharedT9Input.setOnSave(onSave);
    ViewStack::instance().push(&s_sharedT9Input);
    return &s_sharedT9Input;
}

} // namespace cdc::ui
