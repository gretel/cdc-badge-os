#pragma once

#include <cstdint>

class Gdey029T94;

namespace cdc::ui::render {

constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;

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
