---
title: "[MEDIUM] mod_gpg: Internal GPG backend header exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module exposes an internal GPG backend header in its public include directory:

- **`gpg.h`** - Low-level OpenPGP backend functions (internal C-style API)

Only `GpgModule.h` and `GpgStorage.h` should be public. `gpg.h` contains implementation details of the GPG backend.

## Evidence

**Current public include structure:**
```
components/mod_gpg/include/mod_gpg/
├── GpgModule.h       # Public API (correct)
├── GpgStorage.h      # Storage layer (borderline - OK)
├── gpg.h             # GPG backend (internal - exposed!)
└── openpgp/          # Internal subdirectory
    ├── openpgp.h
    ├── apdu.h
    └── ccid.h
```

**Internal header contents:**

`gpg.h` (line 1-70):
```cpp
// GPG backend functions
#define CDC_CURVE_ED25519 0
#define CDC_CURVE_P256    1

#define GPG_USER_ID_MAX         64
#define GPG_FINGERPRINT_LEN     20
#define GPG_FINGERPRINT_V5_LEN  32
#define GPG_PUBKEY_MAX_LEN      64
#define GPG_SIGNATURE_MAX_LEN   64

#define GPG_METADATA_MAGIC      0x4750
#define GPG_METADATA_VERSION    2

typedef struct {
    bool initialized;
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint32_t created_at;
    uint32_t sign_count;
} gpg_status_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint32_t created_at;
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint8_t pubkey[GPG_PUBKEY_MAX_LEN];
    uint8_t pubkey_len;
    uint32_t sign_count;
    uint8_t fingerprint_v5[GPG_FINGERPRINT_V5_LEN];
} gpg_metadata_t;

// GPG functions (internal implementation)
bool gpg_init(void);
bool gpg_is_initialized(void);
bool gpg_get_status(gpg_status_t *status);
bool gpg_set_pending_user_id(const char *user_id);
bool gpg_has_pending_user_id(void);
bool gpg_generate_key(uint8_t curve);
bool gpg_reset(void);
bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len);
bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve);
bool gpg_get_fingerprint(uint8_t *fp_out);
bool gpg_get_fingerprint_v5(uint8_t *fp_out);
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len);
```

**Usage in module:**
```cpp
// From components/mod_gpg/src/GpgModule.cpp:
#include "mod_gpg/gpg.h"

// From components/mod_gpg/src/gpg.cpp:
#include "mod_gpg/gpg.h"
```

**Internal types exposed:**
- `gpg_status_t` - GPG status structure
- `gpg_metadata_t` - GPG metadata structure (packed)
- GPG constants (curve IDs, lengths, magic numbers)

## Impact

- **Implementation Leakage**: External modules can depend on internal GPG backend
- **Tight Coupling**: Changes to GPG metadata format break external consumers
- **Poor Encapsulation**: No clear distinction between public API and internal implementation
- **Namespace Pollution**: Internal types and constants clutter the public API

## Recommended Fix

1. **Move gpg.h to src/openpgp/**:
   ```
   components/mod_gpg/
   ├── include/mod_gpg/
   │   ├── GpgModule.h     # Public API
   │   └── GpgStorage.h    # Storage layer
   └── src/
       ├── GpgModule.cpp
       ├── gpg.h           # Move here (internal)
       ├── gpg.cpp
       └── openpgp/
           ├── openpgp.h
           └── ...
   ```

2. **Update internal includes**:
   ```cpp
   // In src/GpgModule.cpp, change:
   #include "mod_gpg/gpg.h"  # → #include "gpg.h"
   
   // In src/gpg.cpp:
   #include "gpg.h"          # Same directory
   ```

3. **Add internal marker**:
   ```cpp
   // In src/gpg.h:
   /**
    * @file gpg.h
    * @brief Internal GPG backend - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `GpgModule.h`:
   ```cpp
   /**
    * @defgroup gpg-public Public API
    * @brief GPG module - OpenPGP key management
    * 
    * Public classes:
    * - @ref GpgModule - Main module interface
    * - @ref GpgStorage - Storage layer
    */
   ```

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- OpenPGP specification: https://gnupg.org/ftp/specs/OpenPGP-smart-card-application-3.4.pdf

(End of file - total 158 lines)
