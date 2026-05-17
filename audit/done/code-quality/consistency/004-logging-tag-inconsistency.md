---
title: "[MEDIUM] Inconsistent logging TAG definition across module files"
severity: MEDIUM
domain: code-quality/consistency
lens: logging-patterns
labels:
  - "audit:code-quality/consistency"
---

## Summary
Module source files use inconsistent patterns for logging TAG definitions:
- Some files define `static const char* TAG`
- Others use string literals directly in LOG macros
- FIDO2 C-style files use hardcoded string literals instead of TAG variable

## Impact
- **Maintenance friction**: Changing module name requires multiple edits
- **Inconsistent output**: TAG names may use different naming conventions
- **Type safety**: Using const variable is more type-safe than string literals
- **Code duplication**: String literals repeated in multiple places

## Evidence
**Pattern A - TAG variable defined** (preferred):
```cpp
// components/mod_ble_serial/src/BleSerialModule.cpp
static const char* TAG = "BLE_SERIAL";

// components/mod_gpg/src/GpgModule.cpp
static const char* TAG = "GPG";

// components/mod_gpg/src/GpgStorage.cpp
static const char* TAG = "GPGStorage";
```

**Pattern B - No TAG, direct string literals** (inconsistent):
```cpp
// components/mod_fido2/src/ctap2.cpp
LOG_I("CTAP2", "Building authData: pin_verified=%d, cred_protect=%u", ...);
LOG_I("CTAP2", "UV flag SET -> flags=0x%02X", flags);

// components/mod_fido2/src/cbor_helpers.cpp
LOG_E("CBOR", "Write overflow");
LOG_E("CBOR", "Indefinite length not supported");

// components/mod_fido2/src/ctaphid.cpp
// Uses LOG_E("FIDO2", ...) directly without TAG variable
```

**Files in mod_fido2 missing TAG definition**:
- `src/cbor_helpers.cpp`
- `src/ctap2.cpp`
- `src/ctaphid.cpp`
- `src/fido2.cpp`
- `src/fido2_storage.cpp`
- `src/pin_storage.cpp`
- `src/u2f.cpp`

**Files with TAG defined**:
- `src/Fido2Module.cpp` - `static const char* TAG = "FIDO2";`
- `src/Fido2Ui.cpp` - `static const char* TAG = "FIDO2_UI";`

**Inconsistent TAG naming**:
- Some use module name: `TAG = "GPG"`, `TAG = "TOTP"`
- Some use class name: `TAG = "GpgStorage"`, `TAG = "BleHID"`
- Some use underscore: `TAG = "BLE_SERIAL"`, `TAG = "FIDO2_UI"`

## Recommended Fix
1. **Add TAG variable to all FIDO2 C-style files**:

```cpp
// components/mod_fido2/src/ctap2.cpp
#include "mod_fido2/ctap2.h"
// ... includes ...

static const char* TAG = "CTAP2";  // Add this

// Now use TAG instead of "CTAP2" literal:
LOG_I(TAG, "Building authData: pin_verified=%d, cred_protect=%u", ...);
```

2. **Standardize TAG naming convention** (choose one):
   - Option A: Module-level TAG for all files
     ```cpp
     static const char* TAG = "FIDO2";  // All FIDO2 files use "FIDO2"
     ```
   - Option B: File-specific but consistent format
     ```cpp
     static const char* TAG = "FIDO2_CTAP2";    // Module_File format
     static const char* TAG = "FIDO2_CBOR";     // Module_File format
     ```

3. **Update all LOG calls** to use `TAG` variable:
   ```cpp
   // Before
   LOG_I("CTAP2", "message");
   
   // After
   LOG_I(TAG, "message");
   ```

## References
- ESP-IDF logging conventions recommend TAG variable
- cdc_log library documentation
- Consistent with mod_ble_serial, mod_gpg, mod_hid patterns
