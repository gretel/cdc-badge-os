---
title: "[LOW] Inconsistent extern \"C\" block style in module headers"
severity: LOW
domain: code-quality/consistency
lens: header-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
Module headers use two different patterns for `extern "C"` blocks: some wrap entire file contents while others only declare the register function at the end.

**Pattern A - Full file wrapper:**
- `mod_fido2/cbor_helpers.h`
- `mod_fido2/ctap2.h`
- `mod_fido2/ctaphid.h`
- `mod_fido2/fido2.h`
- `mod_fido2/fido2_storage.h`
- `mod_fido2/pin_storage.h`
- `mod_fido2/u2f.h`
- `mod_gpg/GpgStorage.h`
- `mod_gpg/gpg.h`
- `mod_vcard/ble_vcard.h`

**Pattern B - Register function only:**
- `mod_ble_serial/BleSerialModule.h`
- `mod_fido2/Fido2Module.h`
- `mod_gpg/GpgModule.h`
- `mod_hid/HidModule.h`
- `mod_nvsedit/NvsEditModule.h`
- `mod_password/PasswordModule.h`
- `mod_totp/TotpModule.h`
- `mod_vcard/VcardModule.h`

## Impact
- **Style inconsistency**: Two patterns for the same purpose
- **C++ interop clarity**: Pattern A is more explicit for C compatibility
- **Maintenance**: Developers need to know which pattern each file uses

## Evidence
Pattern A (full wrapper) - `components/mod_fido2/include/mod_fido2/fido2.h:11`:
```cpp
extern "C" {
#include <stdint.h>
// ... all includes and declarations ...
}
```

Pattern B (register only) - `components/mod_gpg/include/mod_gpg/GpgModule.h:30`:
```cpp
namespace cdc::mod_gpg {
class GpgModule { /* ... */ };
} // namespace cdc::mod_gpg

extern "C" void mod_gpg_register();
```

## Recommended Fix
Adopt **Pattern B** (register function only) as the standard since:
1. Most module headers use it (8 vs 10 files, but Pattern B is cleaner for C++ modules)
2. Module register functions are the only C-facing API
3. Internal headers can use Pattern A if C compatibility is needed

Update Pattern A files to move `extern "C"` only around the register function declaration.

## References
- C++ header style guide: https://isocpp.org/wiki/faq/mixing-c-and-cpp
