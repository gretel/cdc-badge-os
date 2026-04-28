---
title: "[LOW] Inconsistent Doxygen documentation style in headers"
severity: LOW
domain: code-quality/consistency
lens: documentation-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The project's Doxygen documentation style is inconsistent. The guidelines specify backslash commands (`\brief`, `\param`, `\return`) but some files use at-sign style (`@brief`, `@param`, `@return`).

## Impact
- **Documentation generation**: Some tools prefer one style over the other
- **Developer friction**: Need to remember which style to use
- **Inconsistent appearance**: Generated docs may look different

## Evidence
**Correct style (backslash)** - `components/cdc_log/include/cdc_log.h:95`:
```cpp
/**
 * Output hook - called for every console output
 * @param data Output data
 * @param len Data length
 */
```
(Note: This file actually uses `@param` despite the guidelines saying to use `\param`)

**Expected style per guidelines** - `CLAUDE.md`:
```cpp
/**
 * \brief Send notification on a characteristic.
 * \param connHandle Connection handle (0xFFFF for all).
 * \return true if notification sent.
 */
```

**Inconsistent usage found in mod_fido2/fido2_common.h**:
```cpp
namespace cdc {
namespace mod_fido2 {
// Uses nested namespace style instead of cdc::mod_fido2
} // namespace mod_fido2
} // namespace cdc
```

## Recommended Fix
1. Choose one style (recommended: backslash `\brief`, `\param`, `\return` as per CLAUDE.md)
2. Search and replace in all header files:
   - `@brief` → `\brief`
   - `@param` → `\param`
   - `@return` → `\return`
3. Add pre-commit hook to enforce style

## References
- Project guidelines: `CLAUDE.md` - "Code Quality Requirements" section
- Doxygen documentation: https://www.doxygen.nl/manual/commands.html
