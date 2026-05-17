/**
 * RenderHelpers
 *
 * Shared rendering utilities for common UI chrome and dialogs.
 */

#include "cdc_views/RenderHelpers.h"
#include <goodisplay/gdey029T94.h>
#include <algorithm>

namespace cdc::ui::render {

/**
 * \brief Draws a left-aligned header with optional underline.
 * \param gfx Display drawing context.
 * \param title Optional title text.
 * \param x Header text X position.
 * \param y Header text Y position.
 * \param width Width used for underline.
 * \param underlineOffset Vertical offset for underline.
 * \return void
 */
void drawHeaderLeft(Gdey029T94* gfx, const char* title, int x, int y,
                    uint16_t width, int underlineOffset) {
    if (!gfx) return;

    if (title && title[0] != '\0') {
        gfx->setCursor(x, y);
        gfx->print(title);
    }
    gfx->drawFastHLine(0, y + underlineOffset, width, EPD_BLACK);
}

/**
 * \brief Draws a centered header title.
 * \param gfx Display drawing context.
 * \param title Title text.
 * \param y Header text Y position.
 * \param width Total layout width.
 * \return void
 */
void drawHeaderCentered(Gdey029T94* gfx, const char* title, int y, uint16_t width) {
    if (!gfx || !title || title[0] == '\0') return;

    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor((width - w) / 2, y);
    gfx->print(title);
}

/**
 * \brief Draws footer bar with optional prefix and hint text.
 * \param gfx Display drawing context.
 * \param width Display width.
 * \param height Display height.
 * \param prefix Optional prefix text.
 * \param hint Optional hint text.
 * \param force Draw even if prefix/hint are empty.
 * \return void
 */
void drawFooterBar(Gdey029T94* gfx, uint16_t width, uint16_t height,
                   const char* prefix, const char* hint, bool force) {
    if (!gfx) return;
    if (!force && !prefix && !hint) return;

    gfx->fillRect(0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT, EPD_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(EPD_WHITE);
    gfx->setCursor(4, height - 12);

    if (prefix) {
        gfx->print(prefix);
    }
    if (hint) {
        gfx->print(hint);
    }
}

/**
 * \brief Draws scroll arrows and scrollbar thumb.
 * \param gfx Display drawing context.
 * \param x Indicator X position.
 * \param y Indicator Y position.
 * \param listHeight Height of the scrollable list area.
 * \param totalItems Total item count.
 * \param visibleItems Number of visible items.
 * \param scrollPos Current scroll offset.
 * \return void
 */
void drawScrollIndicator(Gdey029T94* gfx, int x, int y, int listHeight,
                         uint16_t totalItems, uint16_t visibleItems,
                         uint16_t scrollPos) {
    if (!gfx) return;
    if (totalItems <= visibleItems) return;

    gfx->fillRect(x, y, SCROLL_INDICATOR_WIDTH, listHeight, EPD_WHITE);

    const int midX = x + (SCROLL_INDICATOR_WIDTH / 2);
    const int leftX = x + 1;
    const int rightX = x + SCROLL_INDICATOR_WIDTH - 1;

    if (scrollPos > 0) {
        const int topY = y + 4;
        gfx->fillTriangle(
            midX, topY,
            leftX, topY + 6,
            rightX, topY + 6,
            EPD_BLACK
        );
    }

    if (scrollPos + visibleItems < totalItems) {
        const int arrowY = y + listHeight - 12;
        gfx->fillTriangle(
            midX, arrowY + 8,
            leftX, arrowY + 2,
            rightX, arrowY + 2,
            EPD_BLACK
        );
    }

    const int barHeight = listHeight - 24;
    if (barHeight <= 0) return;

    const int barX = x + (SCROLL_INDICATOR_WIDTH / 2) - 2;
    const int barY = y + 12;
    int thumbHeight = std::max(10, barHeight * static_cast<int>(visibleItems) /
                                     static_cast<int>(totalItems));
    int scrollRange = static_cast<int>(totalItems - visibleItems);
    int thumbPos = scrollRange > 0 ? (barHeight - thumbHeight) *
                                     static_cast<int>(scrollPos) / scrollRange
                                   : 0;

    gfx->drawRect(barX, barY, 4, barHeight, EPD_BLACK);
    gfx->fillRect(barX, barY + thumbPos, 4, thumbHeight, EPD_BLACK);
}

/**
 * \brief Draws a framed dialog box with double border.
 * \param gfx Display drawing context.
 * \param x Left position.
 * \param y Top position.
 * \param w Frame width.
 * \param h Frame height.
 * \return void
 */
void drawDialogFrame(Gdey029T94* gfx, int x, int y, int w, int h) {
    if (!gfx) return;

    gfx->fillRect(x, y, w, h, EPD_WHITE);
    gfx->drawRect(x, y, w, h, EPD_BLACK);
    gfx->drawRect(x + 1, y + 1, w - 2, h - 2, EPD_BLACK);
}

} // namespace cdc::ui::render
