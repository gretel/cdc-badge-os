#pragma once

/**
 * \brief Centralized layout constants for cdc_views.
 *
 * Shared display layout values used by multiple views and the
 * RenderHelpers utility. Naming convention: SCREAMING_SNAKE_CASE,
 * matching the rest of the cdc_core/cdc_hal/cdc_ui/cdc_views codebase.
 *
 * View-local constants (TITLE_Y, HINT_Y, TEXT_MARGIN, ITEM_HEIGHT, etc.)
 * intentionally remain in their respective views because their values
 * differ per view (for example TITLE_Y is 5 in ListView but 20 in
 * DateInputView).
 */

namespace cdc::ui::layout {

/**
 * \brief Footer bar height in pixels (used by drawFooterBar).
 */
constexpr int FOOTER_HEIGHT = 16;

/**
 * \brief Scroll indicator column width in pixels.
 */
constexpr int SCROLL_INDICATOR_WIDTH = 8;

} // namespace cdc::ui::layout
