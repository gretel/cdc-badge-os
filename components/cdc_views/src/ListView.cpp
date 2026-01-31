/**
 * ListView Implementation
 *
 * Highly reusable scrollable selection menu.
 * Display is 296x128, VISIBLE_ITEMS is fixed at 4.
 */

#include "cdc_views/ListView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <goodisplay/gdey029T94.h>
#include <algorithm>

static const char* TAG = "ListView";

// Display layout constants (fixed for 296x128 display)
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int FOOTER_HEIGHT = 16;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;

// Available height: 128 - 30 (header) - 16 (footer) = 82px
// With itemHeight_=18: 82/18 = 4 items visible
static constexpr uint8_t VISIBLE_ITEMS = 4;

namespace cdc::ui {

void ListView::init(const char* title, const ListItem* items, uint16_t count) {
    title_ = title;
    items_ = items;
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;

    // Only reset position if not preserving (for back-navigation)
    if (!preservePosition_) {
        selection_ = 0;
        scrollPos_ = 0;
    } else {
        preservePosition_ = false;
        if (selection_ >= itemCount_) {
            selection_ = itemCount_ > 0 ? itemCount_ - 1 : 0;
        }
        ensureVisible();
    }
    visibleItems_ = VISIBLE_ITEMS;
    dirty_ = true;

    LOG_D(TAG, "init: title='%s', items=%d, visible=%d", title, itemCount_, visibleItems_);
}

void ListView::setSelection(uint16_t index) {
    if (index < itemCount_ && index != selection_) {
        selection_ = index;
        ensureVisible();
        dirty_ = true;
    }
}

const ListItem* ListView::getSelectedItem() const {
    if (items_ && selection_ < itemCount_) {
        return &items_[selection_];
    }
    return nullptr;
}

void ListView::navigate(bool down) {
    if (itemCount_ == 0) return;

    if (down) {
        if (selection_ < itemCount_ - 1) {
            selection_++;
        } else {
            // Wrap-around: bottom to top
            selection_ = 0;
            scrollPos_ = 0;
        }
    } else {
        if (selection_ > 0) {
            selection_--;
        } else {
            // Wrap-around: top to bottom
            selection_ = itemCount_ - 1;
        }
    }

    ensureVisible();
    dirty_ = true;

    LOG_D(TAG, "navigate: sel=%d, scroll=%d", selection_, scrollPos_);
}

void ListView::ensureVisible() {
    if (selection_ >= scrollPos_ + visibleItems_) {
        scrollPos_ = selection_ - visibleItems_ + 1;
    }
    if (selection_ < scrollPos_) {
        scrollPos_ = selection_;
    }
}

InputResult ListView::onKey(char key) {
    switch (key) {
        case '2': // Up
            navigate(false);
            return InputResult::CONSUMED;

        case '8': // Down
            navigate(true);
            return InputResult::CONSUMED;

        case 'Y': // Select
            if (onSelect_ && items_ && selection_ < itemCount_) {
                onSelect_(selection_, items_[selection_].userData);
            }
            return InputResult::CONSUMED;

        case '3': // Context menu
            if (onMenu_ && items_ && selection_ < itemCount_) {
                onMenu_(selection_, items_[selection_].userData);
                return InputResult::CONSUMED;
            }
            return InputResult::IGNORED;

        case 'N': // Back
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}

const char* ListView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    return tr(StringId::HINT_OK_BACK);
}

void ListView::render(bool partial) {
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
    gfx->setCursor(ITEM_PADDING_X, TITLE_Y);
    if (title_) {
        gfx->print(title_);
    }
    gfx->drawFastHLine(0, TITLE_Y + 18, width, EPD_BLACK);

    // Items
    for (uint8_t i = 0; i < visibleItems_; i++) {
        uint16_t itemIndex = scrollPos_ + i;
        int y = LIST_START_Y + i * itemHeight_;

        // Clear item area
        gfx->fillRect(0, y, width - SCROLL_INDICATOR_WIDTH, itemHeight_, EPD_WHITE);

        if (itemIndex >= itemCount_) continue;

        const ListItem& item = items_[itemIndex];

        if (itemIndex == selection_) {
            gfx->fillRect(2, y + 1, width - SCROLL_INDICATOR_WIDTH - 4, itemHeight_ - 2, EPD_BLACK);
            gfx->setTextColor(EPD_WHITE);
        } else {
            gfx->setTextColor(EPD_BLACK);
        }

        gfx->setCursor(ITEM_PADDING_X, y + 4);
        if (item.label) {
            gfx->print(item.label);
        }
    }

    // Scroll indicators
    if (itemCount_ > visibleItems_) {
        int indicatorX = width - SCROLL_INDICATOR_WIDTH;
        int listHeight = visibleItems_ * itemHeight_;

        gfx->fillRect(indicatorX, LIST_START_Y, SCROLL_INDICATOR_WIDTH, listHeight, EPD_WHITE);

        // Up arrow
        if (scrollPos_ > 0) {
            gfx->fillTriangle(
                indicatorX + 4, LIST_START_Y + 4,
                indicatorX + 1, LIST_START_Y + 10,
                indicatorX + 7, LIST_START_Y + 10,
                EPD_BLACK
            );
        }

        // Down arrow
        if (scrollPos_ + visibleItems_ < itemCount_) {
            int arrowY = LIST_START_Y + listHeight - 12;
            gfx->fillTriangle(
                indicatorX + 4, arrowY + 8,
                indicatorX + 1, arrowY + 2,
                indicatorX + 7, arrowY + 2,
                EPD_BLACK
            );
        }

        // Scroll bar
        int barHeight = listHeight - 24;
        int thumbHeight = std::max(10, barHeight * visibleItems_ / itemCount_);
        int scrollRange = itemCount_ - visibleItems_;
        int thumbPos = scrollRange > 0 ? (barHeight - thumbHeight) * scrollPos_ / scrollRange : 0;

        gfx->drawRect(indicatorX + 2, LIST_START_Y + 12, 4, barHeight, EPD_BLACK);
        gfx->fillRect(indicatorX + 2, LIST_START_Y + 12 + thumbPos, 4, thumbHeight, EPD_BLACK);
    }

    // Footer with position counter
    gfx->fillRect(0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
    gfx->setCursor(4, height - 12);

    if (itemCount_ > 0) {
        char positionStr[16];
        snprintf(positionStr, sizeof(positionStr), "%u/%u  ", selection_ + 1, itemCount_);
        gfx->print(positionStr);
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

static ListView s_sharedListView;

ListView* showListView(const char* title, const ListItem* items, uint16_t count,
                       ListView::SelectCallback onSelect, const char* hint) {
    s_sharedListView.init(title, items, count);
    s_sharedListView.setOnSelect(onSelect);
    if (hint) {
        s_sharedListView.setHint(hint);
    }
    ViewStack::instance().push(&s_sharedListView);
    return &s_sharedListView;
}

} // namespace cdc::ui
