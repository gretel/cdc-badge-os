---
title: "[MEDIUM] NVS namespace/key constants scattered across modules"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
NVS (Non-Volatile Storage) namespace and key constants are defined inline in multiple module files rather than being centralized. This creates risk of namespace collisions and makes it difficult to understand the overall data layout.

## Impact
1. **Namespace collisions**: Different modules might accidentally use same namespace/key
2. **Data layout unclear**: No single place to see all NVS data structure
3. **Migration difficulty**: Changing namespace names requires finding all occurrences
4. **Documentation gap**: No clear schema of what data is stored where

## Evidence
**NVS namespaces scattered across files:**

| Namespace | Key | Location | Purpose |
|-----------|-----|----------|---------|
| "attest" | "pubhash" | `components/cdc_core/src/AttestationKeyService.cpp:15` | Attestation public key hash |
| "modules" | "list", "disabled" | `components/cdc_core/src/ModuleRegistry.cpp:20-22` | Module configuration |
| "tr01_meta" | "hdr" | `components/cdc_core/src/TropicStorage.cpp:15` | TROPIC01 metadata header |
| "openpgp" | (various) | `components/mod_gpg/src/openpgp/openpgp.cpp:10` | OpenPGP data |
| "sleep" | "interval" | `components/cdc_hal/src/SleepController.cpp:21` | Sleep interval |
| (various) | (various) | `components/mod_nvsedit/src/NvsEditModule.cpp` | NVS editor |

**Code examples:**
```cpp
// components/cdc_core/src/AttestationKeyService.cpp:15-16
static constexpr const char* NVS_NAMESPACE = "attest";
static constexpr const char* NVS_KEY_PUBHASH = "pubhash";

// components/cdc_core/src/ModuleRegistry.cpp:20-22
static constexpr const char* MODULES_NVS_NAMESPACE = "modules";
static constexpr const char* MODULES_NVS_KEY = "list";
static constexpr const char* MODULES_NVS_KEY_DISABLED = "disabled";

// components/cdc_core/src/TropicStorage.cpp:15-16
static constexpr const char* NVS_NAMESPACE = "tr01_meta";
static constexpr const char* NVS_KEY_HEADER = "hdr";

// components/mod_gpg/src/openpgp/openpgp.cpp:10
#define NVS_NAMESPACE "openpgp"

// components/cdc_hal/src/SleepController.cpp:21
static constexpr const char* NVS_KEY_INTERVAL = "interval";
```

**Module registry prefix:**
```cpp
// components/cdc_core/include/cdc_core/ModuleRegistry.h:15
static constexpr const char* NVS_PREFIX = "mod_";
```

## Recommended Fix
1. **Create centralized NVS configuration header**:
   ```cpp
   // components/cdc_core/include/cdc_core/NvsConfig.h
   #pragma once
   
   namespace cdc::nvs {
   
   // === Core namespaces ===
   namespace core {
   constexpr const char* MODULES = "modules";
   constexpr const char* MODULES_KEY_LIST = "list";
   constexpr const char* MODULES_KEY_DISABLED = "disabled";
   
   constexpr const char* ATTESTATION = "attest";
   constexpr const char* ATTESTATION_PUBHASH = "pubhash";
   
   constexpr const char* TROPIC_META = "tr01_meta";
   constexpr const char* TROPIC_HEADER = "hdr";
   }
   
   // === Module namespaces (prefix: "mod_") ===
   namespace modules {
   constexpr const char* GPG = "mod_gpg";
   constexpr const char* TOTP = "mod_totp";
   constexpr const char* PASSWORD = "mod_password";
   constexpr const char* FIDO2 = "mod_fido2";
   constexpr const char* VCARD = "mod_vcard";
   }
   
   // === System namespaces ===
   namespace system {
   constexpr const char* SLEEP = "sleep";
   constexpr const char* SLEEP_INTERVAL = "interval";
   constexpr const char* PIN = "pins";
   }
   
   // === Keys ===
   namespace keys {
   constexpr const char* INTERVAL = "interval";
   constexpr const char* LIST = "list";
   constexpr const char* DISABLED = "disabled";
   constexpr const char* PUBHASH = "pubhash";
   constexpr const char* HEADER = "hdr";
   }
   
   } // namespace cdc::nvs
   ```

2. **Update modules to use centralized config**:
   ```cpp
   // Instead of:
   static constexpr const char* NVS_NAMESPACE = "attest";
   static constexpr const char* NVS_KEY_PUBHASH = "pubhash";
   
   // Use:
   #include "cdc_core/NvsConfig.h"
   static constexpr const char* NVS_NAMESPACE = cdc::nvs::core::ATTESTATION;
   static constexpr const char* NVS_KEY_PUBHASH = cdc::nvs::keys::PUBHASH;
   ```

3. **Create NVS schema documentation**:
   ```markdown
   ## NVS Data Layout
   
   ### Core namespaces
   - `modules`: Module registry data
   - `attest`: Attestation key metadata
   - `tr01_meta`: TROPIC01 metadata
   
   ### Module namespaces (mod_*)
   - `mod_gpg`: OpenPGP smartcard data
   - `mod_totp`: TOTP accounts
   - `mod_password`: Password vault
   - `mod_fido2`: FIDO2 credentials
   
   ### System namespaces
   - `sleep`: Sleep configuration
   - `pins`: PIN state
   ```

## References
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html)
- [Configuration Schema Patterns](https://martinfowler.com/articles/schema-less.html)
