---
title: "[LOW] Doxygen Comment Style Inconsistency"
severity: LOW
domain: documentation
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent Doxygen command syntax:

1. **cdc_core/cdc_ui/cdc_hal**: Backslash style (e.g., `\brief`, `\param`, `\return`)
2. **CalEPD**: At-sign style (e.g., `@brief`, `@param`, `@return`)

Both styles are valid Doxygen syntax, but mixing them creates visual inconsistency.

### Evidence

**cdc_core (backslash style - consistent):**
```cpp
// components/cdc_core/src/AttestationKeyService.cpp:15
/**
 * \brief Initializes service state.
 * \return `true` if service is initialized.
 */

// components/cdc_core/src/ModuleRegistry.cpp:15
/**
 * \brief Returns the singleton module registry instance.
 * \return Registry singleton reference.
 */

// components/cdc_core/src/AttestationKeyService.cpp:60
/**
 * \brief Loads stored public-key hash from NVS.
 * \param out Output hash buffer.
 * \param outLen Expected hash length.
 */
```

**CalEPD (at-sign style - inconsistent):**
```cpp
// components/CalEPD/models/gdem029E97.cpp:385
/**
 * @brief Sets private _mode. When true is monochrome mode
 */

// components/CalEPD/models/gdew042t2Grays.cpp:526
/**
 * @param x
 * @param y
 * @param color
 */

// components/CalEPD/epd.cpp:95
/**
 * @brief Similar to printf
 * @param format
 * @param ... va_list
 */

// components/CalEPD/models/goodisplay/gdeq037T31.cpp:187
/**
 * @brief This is horrible like this since it's raising CS pin for EVERY byte sent
 */
```

**cdc_log (mixed - at-sign style):**
```cpp
// components/cdc_log/include/cdc_log.h:93
/**
 * @param data Output data
 * @param len Data length
 */
```

**Adafruit-GFX (at-sign style):**
```cpp
// components/Adafruit-GFX/Adafruit_GFX.h:114
/**
 * @brief  Set text cursor location
 */
```

## Impact
- **Visual inconsistency**: Different styles in the same codebase
- **Documentation tool consistency**: Some tools may prefer one style
- **Code reviews**: Reviewers may need to check which style to use
- **Project docs recommendation**: "Use `\brief`, `\param`, `\return` - NOT at-sign style"

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt backslash style** (consistent with project docs):
   - `\brief` not `@brief`
   - `\param` not `@param`
   - `\return` not `@return`

2. **Files to fix in CalEPD (scope for ~1 hour fix):**
   - `components/CalEPD/models/gdem029E97.cpp`
   - `components/CalEPD/models/gdew042t2Grays.cpp`
   - `components/CalEPD/models/goodisplay/gdeq037T31.cpp`
   - `components/CalEPD/models/goodisplay/gdey029T94.cpp`
   - `components/CalEPD/epd.cpp`
   - `components/CalEPD/epdspi.cpp`

### Rename pattern:
```cpp
// BEFORE (at-sign style)
/**
 * @brief Sets private _mode. When true is monochrome mode
 * @param x
 * @param y
 * @param color
 */

// AFTER (backslash style)
/**
 * \brief Sets private _mode. When true is monochrome mode
 * \param x
 * \param y
 * \param color
 */
```

## References
- Project docs: "Use `\brief`, `\param`, `\return` - NOT at-sign style"
- Doxygen documentation: Both styles are valid, but consistency matters
- cdc-badge-os project convention: Backslash style used in cdc_core, cdc_ui, cdc_hal

</content>