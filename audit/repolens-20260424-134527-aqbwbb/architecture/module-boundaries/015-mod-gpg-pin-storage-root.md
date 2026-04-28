---
title: "[MEDIUM] mod_gpg: Internal pin_storage.h at root of include directory"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module has an internal storage header at the root of the include directory (not under `mod_gpg/` subdirectory):

- **`components/mod_gpg/include/pin_storage.h`** - PIN storage implementation (internal)

This is a clear boundary violation: the header is at the root level of `include/`, making it easily accessible and appearing as part of the module's public API when it should be internal.

## Impact

- **Implementation Leakage**: PIN storage is easily accessible from anywhere
- **Poor Organization**: Root-level include suggests public API
- **Tight Coupling**: External modules can depend on internal PIN storage
- **Refactoring Risk**: Changes to PIN storage break external consumers

## Evidence

**Current include structure:**
```
components/mod_gpg/include/
├── mod_gpg/
│   ├── GpgModule.h       # Public API (correct)
│   ├── GpgStorage.h      # Storage layer (borderline)
│   ├── gpg.h             # GPG backend (internal)
│   └── openpgp/          # Internal subdirectory
│       ├── openpgp.h
│       ├── apdu.h
│       └── ccid.h
└── pin_storage.h         # INTERNAL at root level!
```

**Internal header contents:**

`pin_storage.h` (line 1-30):
```cpp
// PIN storage for OpenPGP
void pin_storage_openpgp_init(void);
bool pin_storage_openpgp_verify_pw1(const char *pin);
bool pin_storage_openpgp_verify_pw3(const char *pin);
bool pin_storage_openpgp_change_pw1(const char *new_pin);
bool pin_storage_openpgp_change_pw3(const char *new_pin);
uint8_t pin_storage_openpgp_pw1_retries(void);
uint8_t pin_storage_openpgp_pw3_retries(void);
void pin_storage_openpgp_reset_pw1_retries(void);
void pin_storage_openpgp_reset_pw3_retries(void);
bool pin_storage_openpgp_pw1_blocked(void);
bool pin_storage_openpgp_pw3_blocked(void);
bool pin_storage_openpgp_reset(void);
```

**Usage in module:**
```cpp
// From components/mod_gpg/src/openpgp/openpgp.cpp:
#include "pin_storage.h"

// From components/mod_gpg/src/GpgModule.cpp:
#include "pin_storage.h"

// From components/mod_gpg/src/pin_storage.cpp:
#include "pin_storage.h"
```

**Implementation file location:**
```
components/mod_gpg/src/pin_storage.cpp
```

## Recommended Fix

1. **Move pin_storage.h to src/**:
   ```
   components/mod_gpg/
   ├── include/
   │   └── mod_gpg/
   │       ├── GpgModule.h     # Public API
   │       └── GpgStorage.h    # Storage layer
   └── src/
       ├── GpgModule.cpp
       ├── pin_storage.h       # Move here (internal)
       ├── pin_storage.cpp
       └── openpgp/
           └── ...
   ```

2. **Update internal includes**:
   ```cpp
   // In src/openpgp/openpgp.cpp, change:
   #include "pin_storage.h"  # → #include "../pin_storage.h"
   
   // Or better, move pin_storage.h to src/openpgp/ if used only there
   ```

3. **Add internal marker**:
   ```cpp
   // In src/pin_storage.h:
   /**
    * @file pin_storage.h
    * @brief Internal PIN storage - NOT part of public API
    */
   ```

4. **Consider refactoring**:
   - PIN storage is specific to OpenPGP, should be in `src/openpgp/`
   - Or create `src/storage/` directory for storage-related headers

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`

(End of file - total 128 lines)
