/**
 * RenderHelpers
 *
 * Shared rendering utilities for common UI chrome and dialogs.
 */

#include "cdc_views/RenderHelpers.h"
#include <goodisplay/gdey029T94.h>
#include <algorithm>
#include <cstring>

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
void printTruncated(Gdey029T94* gfx, const char* text, int maxWidthPx) {
    if (!gfx || !text || maxWidthPx <= 0) return;

    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    if (static_cast<int>(w) <= maxWidthPx) {
        gfx->print(text);
        return;
    }

    constexpr char ELLIPSIS[] = "...";
    uint16_t ew, eh;
    int16_t ex1, ey1;
    gfx->getTextBounds(ELLIPSIS, 0, 0, &ex1, &ey1, &ew, &eh);

    const int budget = maxWidthPx - static_cast<int>(ew);
    if (budget <= 0) {
        gfx->print(ELLIPSIS);
        return;
    }

    char buf[128];
    size_t len = std::strlen(text);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    std::memcpy(buf, text, len);
    buf[len] = '\0';

    while (len > 0) {
        buf[len] = '\0';
        gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        if (static_cast<int>(w) <= budget) break;
        --len;
    }

    gfx->print(buf);
    gfx->print(ELLIPSIS);
}

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

/**
 * \brief Maps a CP437 byte to the equivalent Latin-1 byte for use with
 *        Latin-1 indexed GFX fonts (TTF-derived range 0x20..0xFF).
 *        ASCII (<0x80) and untracked codes pass through unchanged.
 */
uint8_t cp437ToLatin1(uint8_t c) {
    switch (c) {
        case 0x80: return 0xC7; case 0x81: return 0xFC;
        case 0x82: return 0xE9; case 0x83: return 0xE2;
        case 0x84: return 0xE4; case 0x85: return 0xE0;
        case 0x86: return 0xE5; case 0x87: return 0xE7;
        case 0x88: return 0xEA; case 0x89: return 0xEB;
        case 0x8A: return 0xE8; case 0x8B: return 0xEF;
        case 0x8C: return 0xEE; case 0x8D: return 0xEC;
        case 0x8E: return 0xC4; case 0x8F: return 0xC5;
        case 0x90: return 0xC9; case 0x91: return 0xE6;
        case 0x92: return 0xC6; case 0x93: return 0xF4;
        case 0x94: return 0xF6; case 0x95: return 0xF2;
        case 0x96: return 0xFB; case 0x97: return 0xF9;
        case 0x98: return 0xFF; case 0x99: return 0xD6;
        case 0x9A: return 0xDC; case 0x9B: return 0xA2;
        case 0x9C: return 0xA3; case 0x9D: return 0xA5;
        case 0xA0: return 0xE1; case 0xA1: return 0xED;
        case 0xA2: return 0xF3; case 0xA3: return 0xFA;
        case 0xA4: return 0xF1; case 0xA5: return 0xD1;
        case 0xA6: return 0xAA; case 0xA7: return 0xBA;
        case 0xA8: return 0xBF; case 0xAA: return 0xAC;
        case 0xAB: return 0xBD; case 0xAC: return 0xBC;
        case 0xAD: return 0xA1; case 0xAE: return 0xAB;
        case 0xAF: return 0xBB; case 0xE1: return 0xDF;
        case 0xE6: return 0xB5; case 0xF1: return 0xB1;
        case 0xF6: return 0xF7; case 0xF8: return 0xB0;
        case 0xFD: return 0xB2;
        default: return c;
    }
}

uint8_t unicodeToCp437(uint32_t cp) {
    switch (cp) {
        case 0x00A1: return 0xAD; case 0x00A2: return 0x9B;
        case 0x00A3: return 0x9C; case 0x00A5: return 0x9D;
        case 0x00A6: return 0x7C; case 0x00A7: return 0x15;
        case 0x00AA: return 0xA6; case 0x00AB: return 0xAE;
        case 0x00AC: return 0xAA; case 0x00B0: return 0xF8;
        case 0x00B1: return 0xF1; case 0x00B2: return 0xFD;
        case 0x00B5: return 0xE6; case 0x00BA: return 0xA7;
        case 0x00BB: return 0xAF; case 0x00BC: return 0xAC;
        case 0x00BD: return 0xAB; case 0x00BF: return 0xA8;
        case 0x00C4: return 0x8E; case 0x00C5: return 0x8F;
        case 0x00C6: return 0x92; case 0x00C7: return 0x80;
        case 0x00C9: return 0x90; case 0x00D1: return 0xA5;
        case 0x00D6: return 0x99; case 0x00DC: return 0x9A;
        case 0x00DF: return 0xE1; case 0x00E0: return 0x85;
        case 0x00E1: return 0xA0; case 0x00E2: return 0x83;
        case 0x00E3: return 0x83; case 0x00E4: return 0x84;
        case 0x00E5: return 0x86; case 0x00E6: return 0x91;
        case 0x00E7: return 0x87; case 0x00E8: return 0x8A;
        case 0x00E9: return 0x82; case 0x00EA: return 0x88;
        case 0x00EB: return 0x89; case 0x00EC: return 0x8D;
        case 0x00ED: return 0xA1; case 0x00EE: return 0x8C;
        case 0x00EF: return 0x8B; case 0x00F1: return 0xA4;
        case 0x00F2: return 0x95; case 0x00F3: return 0xA2;
        case 0x00F4: return 0x93; case 0x00F6: return 0x94;
        case 0x00F7: return 0xF6; case 0x00F9: return 0x97;
        case 0x00FA: return 0xA3; case 0x00FB: return 0x96;
        case 0x00FC: return 0x81; case 0x00FF: return 0x98;
        default: return (cp < 0x80) ? static_cast<uint8_t>(cp) : 0;
    }
}

void utf8ToCp437Inplace(char* buf) {
    if (!buf) return;
    uint8_t* r = reinterpret_cast<uint8_t*>(buf);
    uint8_t* w = r;
    while (*r) {
        uint8_t c = *r;
        uint32_t cp = 0;
        uint8_t cont = 0;
        if ((c & 0x80) == 0) { *w++ = c; ++r; continue; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; cont = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; cont = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; cont = 3; }
        else { ++r; continue; }
        ++r;
        bool ok = true;
        for (uint8_t i = 0; i < cont; i++) {
            if ((*r & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (*r & 0x3F);
            ++r;
        }
        if (!ok) continue;
        uint8_t mapped = unicodeToCp437(cp);
        if (mapped) *w++ = mapped;
    }
    *w = '\0';
}

void drawCp437Text(Gdey029T94* gfx, const char* text) {
    if (!gfx || !text) return;
    for (const uint8_t* p = reinterpret_cast<const uint8_t*>(text); *p; ++p) {
        gfx->write(cp437ToLatin1(*p));
    }
}

void measureCp437Text(Gdey029T94* gfx, const char* text, int16_t x0, int16_t y0,
                      int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    if (!gfx || !text) {
        if (x1) *x1 = x0;
        if (y1) *y1 = y0;
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    char buf[128];
    size_t i = 0;
    for (const uint8_t* p = reinterpret_cast<const uint8_t*>(text); *p && i + 1 < sizeof(buf); ++p) {
        buf[i++] = static_cast<char>(cp437ToLatin1(*p));
    }
    buf[i] = '\0';
    gfx->getTextBounds(buf, x0, y0, x1, y1, w, h);
}

} // namespace cdc::ui::render
