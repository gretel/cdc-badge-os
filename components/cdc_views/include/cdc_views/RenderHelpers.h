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

} // namespace cdc::ui::render
