---
title: "[LOW] Multiple modules: Missing public API documentation"
severity: LOW
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

Multiple modules lack clear public API documentation, making it difficult for developers to understand what is intended to be public vs. internal:

1. **mod_totp** - No documentation distinguishing public vs. internal headers
2. **mod_password** - No documentation distinguishing public vs. internal headers
3. **mod_sao** - No documentation distinguishing public vs. internal headers
4. **mod_vcard** - No documentation distinguishing public vs. internal headers
5. **mod_ble_serial** - No documentation distinguishing public vs. internal headers
6. **mod_nvsedit** - No documentation distinguishing public vs. internal headers
7. **mod_hid** - No documentation distinguishing public vs. internal headers

## Impact

- **Developer Confusion**: Unclear what APIs are stable vs. internal
- **Accidental Dependencies**: Developers may depend on internal implementation
- **Refactoring Risk**: Changes to internal code may break external consumers unexpectedly
- **Onboarding Friction**: New developers need to explore code to understand module boundaries

## Evidence

**Example: mod_totp include structure**
```
components/mod_totp/include/mod_totp/
├── TotpModule.h    # Public API?
└── TotpStore.h     # Internal? Public?
```

**Example: mod_password include structure**
```
components/mod_password/include/mod_password/
├── PasswordModule.h    # Public API?
└── PasswordStore.h     # Internal? Public?
```

**Missing documentation pattern:**
```cpp
// Current (no documentation):
// components/mod_totp/include/mod_totp/TotpModule.h
#pragma once
#include "cdc_core/IModule.h"

class TotpModule : public core::IModule {
    // ...
};
```

**Expected documentation pattern:**
```cpp
// Example (with documentation):
// components/mod_totp/include/mod_totp/TotpModule.h
#pragma once
/**
 * @defgroup totp-public Public API
 * @brief TOTP module - Time-based One-Time Password generator
 * 
 * Public classes:
 * - @ref TotpModule - Main module interface
 * 
 * Internal implementation details are in src/
 */
class TotpModule : public core::IModule {
    // ...
};
```

## Recommended Fix

1. **Add public API documentation to each module**:
   ```cpp
   /**
    * @file ModuleName.h
    * @brief Public API for [Module Name] module
    * 
    * This header contains only public API.
    * Internal implementation is in src/
    */
   ```

2. **Add Doxygen groups**:
   ```cpp
   /**
    * @defgroup module-name Public API
    * @brief [Module description]
    * 
    * Main classes:
    * - @ref ModuleName
    */
   ```

3. **Create module README files**:
   - `components/mod_totp/README.md` - Public API overview
   - `components/mod_password/README.md` - Public API overview
   - etc.

4. **Audit each module**:
   - Review all `.h` files in `include/`
   - Move internal headers to `src/`
   - Add documentation to remaining public headers

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules must be: Removable, Isolated, Self-registering"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- Existing good example: `components/cdc_core/include/cdc_core/IModule.h`
