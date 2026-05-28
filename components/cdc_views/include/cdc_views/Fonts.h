#pragma once

#include <cstdint>

#include <gfxfont.h>

namespace cdc::ui {

/**
 * \brief Canonical font identifier shared by firmware UI and plugin host API.
 *
 * The integer values are stable and form the wire format used by the WASM
 * plugin host API (`HOST_FONT_*`). Do not reorder.
 */
enum class FontId : uint8_t {
    Builtin   = 0, ///< Adafruit-GFX 6x8. Umlauts present at CP437 codepoints (0x84..0x9C). Pass raw CP437 bytes to draw them.
    Bold9pt   = 1, ///< FreeMonoBold 9pt. Latin-1 indexed; render via drawCp437Text() to map CP437 input.
    Bold12pt  = 2, ///< FreeMonoBold 12pt. Latin-1 indexed; same mapping path as Bold9pt.
    Bold18pt  = 3, ///< FreeMonoBold 18pt. ASCII only (0x20..0x7E).
    Bold24pt  = 4, ///< FreeMonoBold 24pt. ASCII only (0x20..0x7E).
};

/// Number of entries in the canonical font table.
constexpr uint8_t kFontIdCount = 5;

/**
 * \brief Resolves a \ref FontId to its underlying GFX font pointer.
 * \param id Font identifier.
 * \return Pointer to the GFX font, or `nullptr` for `Builtin` /
 *         out-of-range ids (callers pass `nullptr` to `setFont()` to select
 *         the built-in font).
 */
const GFXfont* getGfxFont(FontId id);

/// Integer overload, for plugin/host-API bridges that ship `uint8_t`.
const GFXfont* getGfxFont(uint8_t id);

} // namespace cdc::ui
