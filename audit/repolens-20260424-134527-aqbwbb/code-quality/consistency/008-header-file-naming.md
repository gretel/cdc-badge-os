---
title: "[MEDIUM] Inconsistent header file naming convention across modules"
severity: MEDIUM
domain: code-quality/consistency
lens: naming-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The project has no unified naming convention for header files. Some modules use PascalCase while others use snake_case, creating cognitive overhead when navigating the codebase.

**PascalCase modules:**
- `mod_ble_serial`: `BleSerialModule.h`, `BleUartService.h`
- `mod_gpg`: `GpgModule.h`, `GpgStorage.h`
- `mod_hid`: `HidModule.h`, `BleHidKeyboard.h`, `KeyboardLayout.h`
- `mod_nvsedit`: `NvsEditModule.h`
- `mod_password`: `PasswordModule.h`, `PasswordStore.h`
- `mod_totp`: `TotpModule.h`, `TotpStore.h`

**snake_case modules:**
- `mod_fido2`: `fido2.h`, `fido2_storage.h`, `pin_storage.h`, `ctap2.h`, `ctaphid.h` (mixed with `Fido2Module.h`, `Fido2Ui.h`)
- `mod_sao`: `sao.h` (mixed with `SaoModule.h`)
- `mod_vcard`: `vcard_store.h`, `ble_vcard.h` (mixed with `VcardModule.h`)

## Impact
- **Developer friction**: Must remember which convention each module uses
- **Search difficulty**: `find . -name "*.h"` returns mixed styles
- **Auto-complete inconsistency**: IDE suggestions vary by module
- **Onboarding overhead**: New developers need to learn multiple patterns

## Evidence
Directory listing comparison:

`components/mod_gpg/include/mod_gpg/`:
```
GpgModule.h      # PascalCase
GpgStorage.h     # PascalCase
gpg.h            # snake_case (exception)
```

`components/mod_fido2/include/mod_fido2/`:
```
Fido2Module.h    # PascalCase (exception)
Fido2Ui.h        # PascalCase (exception)
cbor_helpers.h   # snake_case
ctap2.h          # snake_case
fido2.h          # snake_case
fido2_storage.h  # snake_case
```

## Recommended Fix
Choose one convention and apply it consistently. Recommended: **PascalCase for module headers** (matches C++ class naming):

1. Rename `components/mod_fido2/include/mod_fido2/`:
   - `fido2.h` → `Fido2.h`
   - `fido2_storage.h` → `Fido2Storage.h`
   - `pin_storage.h` → `PinStorage.h`
   - `ctap2.h` → `Ctap2.h`
   - `ctaphid.h` → `Ctaphid.h`
   - `cbor_helpers.h` → `CborHelpers.h`
   - `u2f.h` → `U2f.h`

2. Rename `components/mod_sao/include/mod_sao/`:
   - `sao.h` → `Sao.h`

3. Rename `components/mod_vcard/include/mod_vcard/`:
   - `vcard_store.h` → `VcardStore.h`
   - `ble_vcard.h` → `BleVcard.h`

4. Update all include statements in `.cpp` files

## References
- C++ naming conventions: https://isocpp.org/wiki/faq/coding-conventions
- Existing PascalCase modules: `mod_gpg`, `mod_hid`, `mod_password`
