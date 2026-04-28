---
title: "[MEDIUM] fido2_storage.cpp combines storage layout, slot management, NVS operations, and caching"
severity: MEDIUM
domain: mod_fido2
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/mod_fido2/src/fido2_storage.cpp` (1070 lines) handles multiple distinct responsibilities:
1. **Storage layout** - `fido2_stored_cred_t` struct definition, magic numbers, packing
2. **Slot management** - ECC/R-Memory slot range configuration, mapping logical to physical slots
3. **NVS operations** - Counter persistence, namespace management
4. **Credential caching** - Runtime cache (`g_storage.creds`), cache invalidation
5. **Signature operations** - DER encoding, ECDSA signing helpers
6. **Slot CRUD** - `fido2_storage_save_cred()`, `fido2_storage_load_cred()`, `fido2_storage_delete_cred()`

## Impact
- **High coupling**: Storage format changes, slot mapping, and caching all in same file
- **Complexity**: 1070 lines with mixed concerns makes it hard to navigate
- **Testing difficulty**: Cannot test caching without NVS and slot setup
- **Code duplication**: Similar patterns for read/write/erase could be abstracted

## Evidence
File: `components/mod_fido2/src/fido2_storage.cpp`
- Lines 18-48: Storage layout (`fido2_stored_cred_t` struct)
- Lines 56-78: Runtime cache state (`g_storage` with 20+ fields)
- Lines 80-196: Slot range configuration and mapping helpers
- Lines 198-233: Credential read/write/erase operations
- Lines 235-296: DER signature encoding helpers
- Lines 298-500: Credential save/load logic
- Lines 500-1070: Additional storage operations

Key pattern showing mixed concerns:
```cpp
// Storage layout definition
typedef struct {
    uint8_t magic[FIDO2_RMEM_MAGIC_LEN];    // "FID2"
    uint8_t rp_id_hash[32];                 // SHA-256 of RP ID
    char rp_id[FIDO2_RP_ID_MAX_LEN];        // RP ID string
    uint8_t user_id[FIDO2_USER_ID_MAX_LEN]; // User handle
    // ... 180 bytes total
} fido2_stored_cred_t;

// Runtime cache
static struct {
    bool initialized;
    uint32_t auth_counter;
    struct {
        bool valid;
        uint8_t rp_id_hash[32];
        char rp_id[FIDO2_RP_ID_MAX_LEN];
        // ... cached credential data
    } creds[FIDO2_MAX_CREDENTIALS];
    uint8_t cred_count;
} g_storage = {};

// Slot mapping
static uint8_t ecc_slot_for_logical(uint8_t slot) {
    return static_cast<uint8_t>(s_ecc_start + slot);
}
```

## Recommended Fix
Split into focused modules:
1. **Fido2Layout** - Storage layout definitions in `components/mod_fido2/src/Fido2Layout.cpp`
2. **Fido2SlotMap** - Slot range management in `components/mod_fido2/src/Fido2SlotMap.cpp`
3. **Fido2Cache** - Credential caching in `components/mod_fido2/src/Fido2Cache.cpp`
4. **Fido2Nvs** - NVS counter persistence in `components/mod_fido2/src/Fido2Nvs.cpp`
5. **Fido2Storage** - Main CRUD operations in `components/mod_fido2/src/Fido2Storage.cpp`

Each module should:
- Have its own header file with specific interface
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/mod_fido2/src/
  fido2_storage.cpp  // Main orchestration, delegates to sub-modules
  Fido2Layout.cpp    // Storage layout definitions
  Fido2SlotMap.cpp   // Slot mapping
  Fido2Cache.cpp     // Credential caching
  Fido2Nvs.cpp       // NVS operations
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- FIDO2 Storage Spec: https://fidoalliance.org/specs/fido2/
