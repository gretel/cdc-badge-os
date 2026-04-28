---
title: "[LOW] Inconsistent class naming: PascalCase vs camelCase for module classes"
severity: LOW
domain: code-quality/consistency
lens: naming-conventions
labels:
  - "audit:code-quality/consistency"
---

## Summary
Module classes follow PascalCase convention, but some helper classes use camelCase, creating inconsistency.

## Evidence
**PascalCase (consistent pattern)**:
- `mod_ble_serial`: `BleSerialModule`, `BleUartService`
- `mod_fido2`: `Fido2Module`, `Fido2Ui`
- `mod_gpg`: `GpgModule`, `GpgStorage`
- `mod_hid`: `HidModule`, `BleHidKeyboard`, `KeyboardLayout`
- `mod_nvsedit`: `NvsEditModule`
- `mod_password`: `PasswordModule`, `PasswordStore`
- `mod_sao`: `SaoModule`
- `mod_totp`: `TotpModule`, `TotpStore`
- `mod_vcard`: `VcardModule`

**Inconsistency found in mod_gpg**:
```
include/mod_gpg/
├── GpgModule.h      (PascalCase - class)
├── GpgStorage.h     (PascalCase - class)
└── gpg.h            (camelCase - C-style functions)

src/
├── GpgModule.cpp    (PascalCase)
├── GpgStorage.cpp   (PascalCase)
└── gpg.cpp          (camelCase)
```

**Inconsistency found in mod_fido2**:
```
include/mod_fido2/
├── Fido2Module.h    (PascalCase - class)
├── Fido2Ui.h        (PascalCase - class)
├── cbor_helpers.h   (snake_case - C functions)
├── ctap2.h          (camelCase - C functions)
├── ctaphid.h        (camelCase - C functions)
├── fido2.h          (camelCase - C functions)
└── fido2_storage.h  (snake_case - C functions)
```

**Naming patterns in FIDO2**:
- `cbor_helpers.h` - snake_case (with underscore)
- `ctap2.h` - camelCase (with number)
- `ctaphid.h` - camelCase
- `fido2.h` - camelCase
- `fido2_storage.h` - snake_case

## Impact
- **Cognitive overhead**: Multiple naming conventions to remember
- **Search difficulty**: Harder to find related files
- **Auto-completion**: IDEs may not suggest files consistently

## Recommended Fix
**Establish clear naming convention**:
1. **Module classes**: PascalCase (already consistent)
   - `Fido2Module`, `GpgStorage`, `PasswordStore`

2. **C-style headers**: Choose one convention:
   - Option A: snake_case (recommended for C APIs)
     - `fido2.h`, `fido2_storage.h`, `cbor_helpers.h`
   - Option B: camelCase
     - `fido2.h`, `fido2Storage.h`, `cborHelpers.h`

3. **Implementation files**: Match header naming
   - `fido2.cpp` matches `fido2.h`
   - `GpgModule.cpp` matches `GpgModule.h`

**Suggested standard**:
```
mod_fido2/
├── include/mod_fido2/
│   ├── Fido2Module.h      (class - PascalCase)
│   ├── Fido2Ui.h          (class - PascalCase)
│   ├── fido2.h            (C API - snake_case)
│   ├── fido2_storage.h    (C API - snake_case)
│   ├── ctap2.h            (C API - snake_case, rename from ctap2)
│   ├── ctaphid.h          (C API - snake_case)
│   ├── cbor_helpers.h     (C API - snake_case)
│   └── u2f.h              (C API - snake_case)
```

## References
- Google C++ Style Guide
- ESP-IDF component naming conventions
- Existing pattern in cdc_core (PascalCase for classes)
