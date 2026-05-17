#pragma once

#include <cstdint>

#include "cdc_views/LayoutConstants.h"

class Gdey029T94;

namespace cdc::ui::render {

// Backwards-compatible aliases for the shared layout constants.
// Prefer the canonical names in cdc::ui::layout for new code.
constexpr int FOOTER_HEIGHT = cdc::ui::layout::FOOTER_HEIGHT;
constexpr int SCROLL_INDICATOR_WIDTH = cdc::ui::layout::SCROLL_INDICATOR_WIDTH;

void drawHeaderLeft(Gdey029T94* gfx, const char* title, int x, int y,
                    uint16_t width, int underlineOffset = 18);
void drawHeaderCentered(Gdey029T94* gfx, const char* title, int y, uint16_t width);

void drawFooterBar(Gdey029T94* gfx, uint16_t width, uint16_t height,
                   const char* prefix, const char* hint, bool force = false);

void drawScrollIndicator(Gdey029T94* gfx, int x, int y, int listHeight,
                         uint16_t totalItems, uint16_t visibleItems,
                         uint16_t scrollPos);

void drawDialogFrame(Gdey029T94* gfx, int x, int y, int w, int h);

/**
 * \brief Maps a CP437 byte to the equivalent Latin-1 byte for use with
 *        Unicode/Latin-1 indexed GFX fonts (e.g. FreeMonoBold*pt8b).
 * \param c CP437 byte value.
 * \return Latin-1 byte representing the same character, or `c` when no mapping is needed.
 */
uint8_t cp437ToLatin1(uint8_t c);

/**
 * \brief Prints a CP437 string by mapping each byte to Latin-1 before drawing.
 *        Use with TTF-derived GFX fonts (range 0x20..0xFF) that expect Latin-1 indices.
 * \param gfx Target display.
 * \param text CP437-encoded null-terminated string.
 */
void drawCp437Text(Gdey029T94* gfx, const char* text);

/**
 * \brief Measures a CP437 string using the current font (via Latin-1 mapping).
 * \param gfx Target display.
 * \param text CP437-encoded null-terminated string.
 * \param x0 Starting x position for measurement.
 * \param y0 Starting y position for measurement.
 * \param x1 Output: top-left x of the rendered bounds.
 * \param y1 Output: top-left y of the rendered bounds.
 * \param w  Output: width of the rendered text.
 * \param h  Output: height of the rendered text.
 */
void measureCp437Text(Gdey029T94* gfx, const char* text, int16_t x0, int16_t y0,
                      int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);

} // namespace cdc::ui::render
