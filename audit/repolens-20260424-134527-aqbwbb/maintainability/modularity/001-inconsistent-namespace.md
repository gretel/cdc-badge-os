---
title: "[MEDIUM] Inconsistent namespace usage in mod_gpg module files"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `mod_gpg` module has inconsistent namespace usage across its source files. The files `gpg.cpp` and `openpgp/openpgp.cpp` do not use the `namespace cdc::mod_gpg` wrapper, while `GpgModule.cpp`, `GpgStorage.cpp`, and `pin_storage.cpp` do. This creates a fragmented module boundary where some functions are namespaced and others are in the global namespace.

**Evidence:**
- `mod_gpg/src/GpgModule.cpp` (line 28): `namespace cdc::mod_gpg {`
- `mod_gpg/src/GpgStorage.cpp`: Uses `namespace cdc::mod_gpg`
- `mod_gpg/src/gpg.cpp`: **No namespace wrapper** (60 lines, all global scope)
- `mod_gpg/src/openpgp/openpgp.cpp` (line 148 only): Has namespace reference but no wrapper
- `mod_gpg/src/pin_storage.cpp`: Uses `namespace cdc::mod_gpg`

## Impact
1. **Namespace pollution**: Functions like `gpg_export_pubkey_raw()`, `gpg_sign_hash()`, `gpg_metadata_t` struct are in global scope, increasing collision risk
2. **Inconsistent API surface**: Module consumers must remember which functions are namespaced and which are not
3. **Maintainability burden**: New developers may add files without namespace, worsening the inconsistency
4. **Refactoring difficulty**: Moving/reorganizing code requires checking each file individually for namespace context

## Evidence
**gpg.cpp** (lines 1-60, no namespace):
```cpp
#include "mod_gpg/gpg.h"
#include "mod_gpg/GpgStorage.h"
#include "mod_gpg/openpgp/openpgp.h"
// ... includes ...

static constexpr const char* NVS_NAMESPACE = "mod_gpg";
static bool s_initialized = false;
static gpg_metadata_t s_metadata = {};

bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve) {
    // ... global scope function ...
}
```

**GpgModule.cpp** (lines 28-630, properly namespaced):
```cpp
namespace cdc::mod_gpg {
static uint16_t s_strIdBase = 0;
// ... all code in namespace ...
} // namespace cdc::mod_gpg
```

## Recommended Fix
1. Wrap `gpg.cpp` in `namespace cdc::mod_gpg { }` (approximately 15 minutes)
   - Add `namespace cdc::mod_gpg {` after includes and static variables
   - Add `} // namespace cdc::mod_gpg` at end of file
   - Move static variables inside namespace

2. Wrap `openpgp/openpgp.cpp` in `namespace cdc::mod_gpg { }` (approximately 30 minutes)
   - Check all global functions and structs
   - Add namespace wrapper
   - Update any forward declarations if needed

3. Verify compilation and run any existing tests (approximately 15 minutes)

**Total estimated time: ~1 hour**

## References
- C++ Core Guidelines [N4.1](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-namespaces): Use namespaces to avoid name collisions
- Project pattern: Other modules (mod_totp, mod_password, mod_fido2) consistently use namespace wrappers
