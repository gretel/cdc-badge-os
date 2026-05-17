---
title: "[MEDIUM] Inconsistent Doxygen comment style between core components and modules"
severity: MEDIUM
domain: code-quality/comments
lens: comments
labels:
  - "audit:code-quality/comments"
---

## Summary
The codebase uses two different comment styles for API documentation:

1. **Core components** (cdc_core, cdc_hal, cdc_log, etc.): Use backslash-style Doxygen
   - `\brief`, `\param`, `\return`
   - Example: `components/cdc_core/include/cdc_core/IModule.h`

2. **Adafruit-GFX** (third-party, but mixed in): Use at-sign style
   - `@brief`, `@param`, `@return`
   - Example: `components/Adafruit-GFX/Adafruit_GFX.h`

While third-party code can maintain its own style, the project's documentation guidelines specify backslash style (`\brief` not `@brief`). This creates inconsistency for developers reading across the codebase.

## Impact
- **Developer confusion**: Inconsistent style makes documentation harder to scan
- **Tooling**: Some documentation generators may prefer one style
- **Onboarding**: New developers need to learn both styles
- **Documentation standards**: Project guidelines in `CLAUDE.md` specify backslash style

## Evidence

**Backslash style (preferred)** - `components/cdc_core/include/cdc_core/IModule.h:76`:
```cpp
/**
 * Get module menu items
 * @param items Output array to fill
 * @param maxItems Maximum items to return
 * @return Number of items written
 */
virtual uint8_t getMenuItems(ModuleMenuItem* items, uint8_t maxItems)
```

**At-sign style** - `components/Adafruit-GFX/Adafruit_GFX.h:165`:
```cpp
void drawPixel(int16_t x, int16_t y, uint16_t color) = 0; ///< Virtual drawPixel() function to draw to the
                                                          ///< screen/framebuffer/etc, must be overridden in
                                                          ///< subclass. @param x X coordinate.  @param y Y
                                                          ///< coordinate. @param color 16-bit pixel color.
```

**Mixed styles in same file** - `components/cdc_log/include/cdc_log.h`:
- Line 95: `* @param data Output data` (at-sign style in header block)
- But project guidelines say to use backslash

## Recommended Fix
Standardize on backslash style (`\brief`, `\param`, `\return`) per project documentation:

1. **For new code**: Always use backslash style
2. **For existing core code**: Gradually convert at-sign to backslash when touching files
3. **For third-party**: Leave as-is (external dependencies)

Example conversion:
```cpp
// From:
/** @brief Send notification
 * @param handle Connection handle
 * @return true on success
 */

// To:
/**
 * \brief Send notification
 * \param handle Connection handle
 * \return true on success
 */
```

## References
- Project documentation: `CLAUDE.md` - "Use backslash commands (`\brief`, `\param`, `\return`) - NOT at-sign style (`@brief`, etc.)"
- Doxygen manual: Both styles are valid, but consistency matters
