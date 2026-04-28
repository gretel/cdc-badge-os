---
title: "[MEDIUM] Missing data flow and storage architecture documentation"
severity: MEDIUM
domain: architecture
lens: architecture-docs
labels:
  - "audit:documentation/architecture-docs"
---

## Summary
The system's data flow from user input through processing to storage is not documented. Key gaps include:

1. **No data flow diagrams** showing how data moves through layers
2. **Missing storage documentation** - TROPIC01 R-Memory vs ECC slots vs NVS usage
3. **No data classification** - Which data is PII, encrypted, or public
4. **Unclear encryption layers** - What data is encrypted and where

**Evidence:**
- `main/tropic_slot_map.h` - Contains slot allocations but no explanation of data types stored
- `docs/README.md:107-114` - Brief mention of TROPIC01 capacity but no data flow
- `components/cdc_core/include/cdc_core/TropicStorage.h` - Storage API exists but no usage docs
- No documentation of NVS namespaces and what data is stored where
- Missing explanation of encryption/attestation flow

## Impact
- Developers may store sensitive data in wrong location (NVS vs TROPIC01)
- Hard to understand security boundaries
- Risk of data loss during module development
- Unclear which storage to use for new features
- No reference for backup/restore requirements

## Evidence
**Current slot map (`main/tropic_slot_map.h:28-57`):**
```cpp
// Module IDs (must be unique, 0-254). 255 is reserved for UNKNOWN.
#define MODULE_ID_MOD_SYSTEM 0
#define MODULE_ID_MOD_GPG 2
#define MODULE_ID_MOD_CA 3
#define MODULE_ID_MOD_FIDO2 4
#define MODULE_ID_MOD_TOTP 5
#define MODULE_ID_MOD_PASSWORD 6

// ECC slot ranges
#define ECC_SLOT_MOD_GPG_START 1
#define ECC_SLOT_MOD_GPG_END 3
#define ECC_SLOT_MOD_CA_START 4
#define ECC_SLOT_MOD_CA_END 4
#define ECC_SLOT_MOD_FIDO2_START 5
#define ECC_SLOT_MOD_FIDO2_END 31

// RMEM slot ranges
#define RMEM_SLOT_MOD_TOTP_START 32
#define RMEM_SLOT_MOD_TOTP_END 131
#define RMEM_SLOT_MOD_FIDO2_START 132
#define RMEM_SLOT_MOD_FIDO2_END 158
#define RMEM_SLOT_MOD_PASSWORD_START 159
#define RMEM_SLOT_MOD_PASSWORD_END 511
```

**What's missing:**
- What data is stored in each slot range (e.g., FIDO2: credentials, TOTP: secrets)
- How data is encrypted before storage
- NVS usage (WiFi credentials, settings, user preferences)
- Data lifecycle (creation, encryption, storage, retrieval, deletion)

**Code evidence:**
- `components/cdc_hal/include/cdc_hal/ISecureElement.h` - Secure element interface
- `components/cdc_core/include/cdc_core/TropicStorage.h` - Storage abstraction
- No docs on NVS namespaces (likely used for WiFi, settings, PIN)

## Recommended Fix
Add `docs/data-flow.md` with:

**1. Storage Architecture Overview**
```
┌─────────────────────────────────────────────────────────────┐
│                    Data Flow                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  User Input → Validation → Encryption → Storage            │
│                                                             │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌─────────┐ │
│  │  T9 UI   │ → │  Module  │ → │ Tropic   │ → │ TROPIC01│ │
│  │  Input   │   │  Logic   │   │ Storage  │   │ R-Memory│ │
│  └──────────┘   └──────────┘   └──────────┘   └─────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**2. Storage Locations**
| Data Type | Storage | Encrypted | Key Source |
|-----------|---------|-----------|------------|
| FIDO2 Credentials | TROPIC01 R-Memory | Yes | TROPIC01 ECC slot |
| TOTP Secrets | TROPIC01 R-Memory | Yes | TROPIC01 ECC slot |
| Passwords | TROPIC01 R-Memory | Yes | TROPIC01 ECC slot |
| GPG Keys | TROPIC01 ECC | Hardware | TROPIC01 |
| WiFi Credentials | NVS | Yes | System key |
| User Settings | NVS | No | - |
| PIN | TROPIC01 R-Memory (slot 0) | Yes | System |

**3. TROPIC01 Slot Allocation Details**
| Module | ECC Slots | R-Memory Slots | Data Stored |
|--------|-----------|----------------|-------------|
| SYSTEM | 0 (Attestation) | 0 (PINs, Config) | Device PIN, Attestation key |
| mod_gpg | 1-3 (3 keys) | 1-3 (metadata) | P-256/Ed25519 keys for GPG |
| mod_fido2 | 5-31 (27 keys) | 132-158 (27 creds) | WebAuthn credentials |
| mod_totp | - | 32-131 (100 slots) | TOTP secrets (32 bytes + metadata) |
| mod_password | - | 159-511 (353 slots) | Password entries |

**4. Encryption Flow**
```
1. Module creates data (e.g., TOTP secret)
2. TropicStorage::encrypt() called with module's ECC key
3. TROPIC01 performs AES encryption internally
4. Encrypted data written to R-Memory slot
5. On read: TROPIC01 decrypts, returns plaintext to module
```

**5. Data Classification**
- **High security** (TROPIC01 ECC): Private keys, attestation
- **Medium security** (TROPIC01 R-Memory): Encrypted secrets
- **Low security** (NVS): Settings, display preferences

**6. Backup/Restore Requirements**
- TROPIC01 data: Cannot be exported (hardware-bound)
- NVS data: Can be backed up via NVS editor tool

## References
- TROPIC01 documentation: `third_party/libtropic_sdk/`
- Storage API: `components/cdc_core/include/cdc_core/TropicStorage.h`
- Secure element interface: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
