/**
 * \brief Canonical font table shared by firmware UI and plugin host API.
 */

#include "cdc_views/Fonts.h"

#include <cdc_views/fonts/FreeMonoBold9pt8b.h>
#include <cdc_views/fonts/FreeMonoBold12pt8b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

namespace cdc::ui {

namespace {

const GFXfont* const kFonts[kFontIdCount] = {
    nullptr,                // FontId::Builtin
    &FreeMonoBold9pt8b,     // FontId::Bold9pt
    &FreeMonoBold12pt8b,    // FontId::Bold12pt
    &FreeMonoBold18pt7b,    // FontId::Bold18pt
    &FreeMonoBold24pt7b,    // FontId::Bold24pt
};

} // namespace

const GFXfont* getGfxFont(FontId id) {
    return getGfxFont(static_cast<uint8_t>(id));
}

const GFXfont* getGfxFont(uint8_t id) {
    if (id >= kFontIdCount) return nullptr;
    return kFonts[id];
}

} // namespace cdc::ui
