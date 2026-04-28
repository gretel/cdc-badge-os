---
title: "[HIGH] Inconsistent header guard patterns across module headers"
severity: HIGH
domain: code-quality/consistency
lens: header-organization
labels:
  - "audit:code-quality/consistency"
---

## Summary
The module headers use mixed header guard patterns, creating inconsistency:
- **Primary pattern**: `#pragma once` (used in most files)
- **Secondary pattern**: `#ifndef/#define` guards (used in FIDO2 C-style headers)
- **Missing guards**: Some headers have no guards at all

**Affected files** (missing header guards):
- `components/mod_ble_serial/include/mod_ble_serial/BleSerialModule.h`
- `components/mod_ble_serial/include/mod_ble_serial/BleUartService.h`
- `components/mod_fido2/include/mod_fido2/Fido2Module.h`
- `components/mod_fido2/include/mod_fido2/Fido2Ui.h`
- `components/mod_fido2/include/mod_fido2/pin_storage.h`
- `components/mod_fido2/include/mod_fido2/ctaphid.h` (has `#define` but no `#ifndef`)
- `components/mod_gpg/include/mod_gpg/GpgModule.h`
- `components/mod_gpg/include/mod_gpg/GpgStorage.h`
- `components/mod_gpg/include/mod_gpg/gpg.h`
- `components/mod_hid/include/mod_hid/BleHidKeyboard.h`
- `components/mod_hid/include/mod_hid/HidModule.h`
- `components/mod_hid/include/mod_hid/KeyboardLayout.h`
- `components/mod_nvsedit/include/mod_nvsedit/NvsEditModule.h`
- `components/mod_password/include/mod_password/PasswordModule.h`
- `components/mod_password/include/mod_password/PasswordStore.h`
- `components/mod_sao/include/mod_sao/SaoModule.h`
- `components/mod_sao/include/mod_sao/sao.h`
- `components/mod_totp/include/mod_totp/TotpModule.h`
- `components/mod_totp/include/mod_totp/TotpStore.h`
- `components/mod_vcard/include/mod_vcard/VcardModule.h`
- `components/mod_vcard/include/mod_vcard/ble_vcard.h`
- `components/mod_vcard/include/mod_vcard/vcard_store.h`

**Files with `#ifndef/#define` guards**:
- `components/mod_fido2/include/mod_fido2/cbor_helpers.h`
- `components/mod_fido2/include/mod_fido2/ctap2.h`
- `components/mod_fido2/include/mod_fido2/fido2.h`
- `components/mod_fido2/include/mod_fido2/fido2_common.h`
- `components/mod_fido2/include/mod_fido2/fido2_storage.h`
- `components/mod_fido2/include/mod_fido2/u2f.h`

## Impact
- **Cognitive overhead**: Developers need to recognize multiple patterns
- **Maintenance friction**: New contributors may use different patterns
- **Potential for bugs**: Missing guards can cause compilation issues with forward declarations
- **Inconsistent style**: `ctaphid.h` has `#define CTAPHID_PACKET_SIZE` but no `#ifndef` guard

## Evidence
```cpp
// File: components/mod_fido2/include/mod_fido2/ctaphid.h
// Has define but no guard:
#define CTAPHID_PACKET_SIZE      64
#define CTAPHID_INIT_DATA        57
// ... no #ifndef/#define guard at top

// File: components/mod_fido2/include/mod_fido2/fido2.h
// Uses traditional guards:
#ifndef FIDO2_H
#define FIDO2_H
// ...
#endif

// File: components/mod_ble_serial/include/mod_ble_serial/BleSerialModule.h
// Uses pragma once:
#pragma once
```

## Recommended Fix
1. **Standardize on `#pragma once`** as the primary guard pattern (already used in cdc_core, cdc_ui, cdc_views)
2. **Add `#pragma once` to all headers missing guards**
3. **Migrate FIDO2 C-style headers** from `#ifndef/#define` to `#pragma once` for consistency
4. **Fix `ctaphid.h`** - ensure it has proper guards (currently missing)

Example transformation:
```cpp
// Before (fido2.h):
#ifndef FIDO2_H
#define FIDO2_H
#include <stdint.h>
// ...
#endif

// After (fido2.h):
#pragma once
#include <stdint.h>
// ...
```

## References
- CMakeLists.txt in `main/` generates registration code assuming consistent module structure
- Existing pattern in `components/cdc_core/include/cdc_core/` uses `#pragma once` exclusively
- ESP-IDF style guide recommends consistent header guard usage
