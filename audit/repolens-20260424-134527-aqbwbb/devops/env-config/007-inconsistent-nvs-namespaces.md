---
title: "[LOW] Inconsistent NVS namespace naming convention"
severity: LOW
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
NVS (Non-Volatile Storage) namespaces are defined inconsistently across modules. There is no centralized configuration or naming convention enforcement, leading to:

- Some modules use `mod_` prefix (e.g., `mod_gpg`, `mod_grove_led`)
- Some modules use short names without prefix (e.g., `gpg`, `i18n`, `fido2`)
- Core components use custom names (e.g., `attest`, `tr01_meta`, `modules`)

This inconsistency makes it harder to:
- Document all NVS namespaces in one place
- Audit storage usage across the system
- Migrate or backup NVS data consistently

## Impact
- **Documentation gap**: No single source of truth for all NVS namespaces
- **Maintenance burden**: Hard to track which module uses which namespace
- **Migration complexity**: NVS clearing or migration scripts need to know all namespace names
- **Debugging**: Harder to correlate NVS entries with modules when naming is inconsistent

## Evidence

**Inconsistent naming patterns found:**

| Module | Namespace | Location |
|--------|-----------|----------|
| mod_gpg | `mod_gpg` | `components/mod_gpg/src/gpg.cpp` |
| mod_grove_led | `mod_grove_led` | `components/grove_led/src/GroveLedModule.cpp` |
| mod_hid | `mod_hid` | `components/mod_hid/src/BleHidKeyboard.cpp` |
| gpg (openpgp) | `openpgp` | `components/mod_gpg/src/openpgp/openpgp.cpp` |
| fido2 | `fido2` | `components/mod_fido2/src/fido2_storage.cpp` |
| i18n | `i18n` | `components/cdc_ui/src/I18n.cpp` |
| modules | `modules` | `components/cdc_core/src/ModuleRegistry.cpp` |
| attest | `attest` | `components/cdc_core/src/AttestationKeyService.cpp` |
| tr01_meta | `tr01_meta` | `components/cdc_core/src/TropicStorage.cpp` |
| vcard | `VCARD_NAMESPACE` | `components/mod_vcard/src/vcard_store.cpp` |

**ModuleRegistry defines a prefix but doesn't enforce it:**
File: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
```cpp
static constexpr const char* NVS_PREFIX = "mod_";
```
But this is only used dynamically when constructing namespace names, not as a compile-time constant for all modules.

## Recommended Fix

**Option A: Enforce consistent naming with macros**

1. **Create `components/cdc_core/include/cdc_core/NvsNamespaces.h`**:
```cpp
#pragma once

// Centralized NVS namespace definitions
#define NVS_NAMESPACE_MOD_GPG       "mod_gpg"
#define NVS_NAMESPACE_MOD_FIDO2     "mod_fido2"
#define NVS_NAMESPACE_MOD_TOTP      "mod_totp"
#define NVS_NAMESPACE_MOD_PASSWORD  "mod_password"
#define NVS_NAMESPACE_MOD_GROVE_LED "mod_grove_led"
#define NVS_NAMESPACE_MOD_HID       "mod_hid"
#define NVS_NAMESPACE_MOD_VCARD     "mod_vcard"
#define NVS_NAMESPACE_MOD_BLE_SERIAL "mod_ble_serial"
#define NVS_NAMESPACE_MOD_NVSEDIT   "mod_nvsedit"

// Core namespaces
#define NVS_NAMESPACE_I18N          "i18n"
#define NVS_NAMESPACE_MODULES       "modules"
#define NVS_NAMESPACE_ATTEST        "attest"
#define NVS_NAMESPACE_TR01_META     "tr01_meta"
#define NVS_NAMESPACE_OPENPGP       "openpgp"
```

2. **Update modules to use centralized definitions**:
```cpp
#include "cdc_core/NvsNamespaces.h"
static constexpr const char* NVS_NAMESPACE = NVS_NAMESPACE_MOD_GPG;
```

**Option B: Document existing namespaces**

If full refactoring is too large, at minimum create documentation:
1. Add to `CONFIGURATION.md` a table of all NVS namespaces
2. Document the naming convention (even if not enforced)
3. Add a comment block at the top of each module with its NVS namespace

## References
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html)
- [12-Factor App - Config](https://12factor.net/config)
