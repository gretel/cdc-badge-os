---
title: "[MEDIUM] Missing namespace declarations in mod_fido2 source files"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
Multiple source files in `mod_fido2` use `using cdc::mod_fido2::...` statements to access functions from other files, but don't wrap their own code in `namespace cdc::mod_fido2`. This means functions like `get_se()` and `sha256()` are in the global namespace, creating potential collision risks.

**Evidence:**
- `mod_fido2/src/fido2.cpp` (lines 14-19): Forward declarations with namespace, then `using namespace cdc::mod_fido2;`
- `mod_fido2/src/ctap2.cpp` (line 30-31): `using cdc::mod_fido2::sha256;`
- `mod_fido2/src/u2f.cpp` (lines 17-18): `using cdc::mod_fido2::get_se; using cdc::mod_fido2::sha256;`
- `mod_fido2/src/fido2_storage.cpp` (lines 15-16): `using cdc::mod_fido2::get_se; using cdc::mod_fido2::sha256;`
- `mod_fido2/src/ctaphid.cpp` (line 17): `using cdc::mod_fido2::fido2_usb_write;`

## Impact
1. **Global namespace pollution**: Helper functions like `get_se()`, `sha256()` are not namespaced
2. **Inconsistent API**: Some functions are namespaced, others are global
3. **Forward declaration hack**: `fido2.cpp` uses a hack pattern with forward declarations instead of proper namespace structure
4. **Refactoring difficulty**: Moving functions requires updating all `using` statements

## Evidence
**fido2.cpp** (lines 14-22):
```cpp
// USB transport hooks implemented by Fido2Module.cpp.
namespace cdc::mod_fido2 {
    bool fido2_usb_available();
    bool fido2_usb_ready();
    uint16_t fido2_usb_read(uint8_t* buffer);
    bool fido2_usb_write(const uint8_t* buffer);
}

using namespace cdc::mod_fido2;  // Redundant after above declaration!

/** \brief Global FIDO2 runtime state. */
static struct {
    bool initialized;
    // ...
} g_fido2 = {};
```

**u2f.cpp** (lines 14-60, no namespace wrapper):
```cpp
#include "mod_fido2/u2f.h"
// ... includes ...

using cdc::mod_fido2::get_se;
using cdc::mod_fido2::sha256;

/** \brief DER encoding helper constants. */
static constexpr uint8_t DER_SEQUENCE_TAG = 0x30;

// Functions are in GLOBAL namespace!
static bool u2f_attest_sign(const uint8_t *data, size_t data_len, ...) {
    auto* se = get_se();  // Relies on using statement
    // ...
}
```

**Compare with fido2.h** (properly namespaced):
```cpp
namespace cdc::mod_fido2 {
typedef enum { ... } fido2_action_t;
void fido2_init();
bool fido2_process(...);
}
```

## Recommended Fix
1. Wrap `fido2.cpp` content in `namespace cdc::mod_fido2 { }` (10 minutes)
   - Remove the forward declaration block (use proper includes instead)
   - Remove `using namespace cdc::mod_fido2;`
   - Add namespace wrapper around all code

2. Wrap `u2f.cpp` content in `namespace cdc::mod_fido2 { }` (10 minutes)
   - Remove `using cdc::mod_fido2::get_se;`
   - Remove `using cdc::mod_fido2::sha256;`
   - Add namespace wrapper

3. Wrap `ctap2.cpp` content in `namespace cdc::mod_fido2 { }` (10 minutes)
   - Already has namespace at line 649, verify it wraps all code
   - Remove `using cdc::mod_fido2::sha256;`
   - Remove `using cdc::mod_fido2::sha256_str;`

4. Wrap `fido2_storage.cpp` content in `namespace cdc::mod_fido2 { }` (10 minutes)
   - Remove `using cdc::mod_fido2::get_se;`
   - Remove `using cdc::mod_fido2::sha256;`

5. Wrap `ctaphid.cpp` content in `namespace cdc::mod_fido2 { }` (5 minutes)
   - Remove `using cdc::mod_fido2::fido2_usb_write;`

6. Update `fido2.h` forward declarations if needed (5 minutes)

**Total estimated time: ~50 minutes**

## References
- C++ Core Guidelines [R.10](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-namespace): Use namespaces
- Project pattern: `mod_gpg`, `mod_totp`, `mod_password` all use consistent namespace wrapping
