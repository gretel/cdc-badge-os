---
title: "[MEDIUM] mod_fido2: Internal helper headers exposed in public include directory"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_fido2` module exposes internal helper headers in its public include directory, allowing external consumers to reach into implementation details:

1. **`fido2_common.h`** - Utility functions (SHA-256, secure element getter) exposed at `components/mod_fido2/include/mod_fido2/fido2_common.h`
2. **`cbor_helpers.h`** - CBOR encoding/decoding helpers exposed at `components/mod_fido2/include/mod_fido2/cbor_helpers.h`
3. **`ctaphid.h`**, **`ctap2.h`** - Low-level CTAP protocol headers exposed publicly

These are implementation details, not part of the FIDO2 module's public API.

## Impact

- **Implementation Leakage**: External modules can depend on internal helper functions
- **Tight Coupling**: Changes to CBOR encoding or FIDO2 internals break external consumers
- **Namespace Pollution**: Internal types and functions clutter the public API
- **Refactoring Difficulty**: Internal changes require checking all external dependencies

## Evidence

**Current public include structure:**
```
components/mod_fido2/include/mod_fido2/
├── Fido2Module.h         # Public API (correct)
├── Fido2Ui.h             # UI layer (maybe public?)
├── fido2.h               # Core FIDO2 (internal?)
├── fido2_common.h        # Internal helpers (exposed!)
├── fido2_storage.h       # Storage (internal)
├── cbor_helpers.h        # CBOR encoding (internal!)
├── ctaphid.h             # CTAP HID (internal)
├── ctap2.h               # CTAP2 protocol (internal)
├── fido2_common.h        # Common helpers (internal!)
├── pin_storage.h         # PIN storage (internal)
└── u2f.h                 # U2F protocol (internal)
```

**Internal helper usage:**
```cpp
// From components/mod_fido2/include/mod_fido2/fido2_common.h (line 7-18):
#include <cdc_hal/ISecureElement.h>
#include <mbedtls/sha256.h>

namespace cdc::mod_fido2 {
    inline hal::ISecureElement* get_se() { ... }
    inline void sha256(const uint8_t* data, size_t len, uint8_t out[32]) { ... }
    inline void sha256_str(const char* str, uint8_t out[32]) { ... }
}
```

**CBOR helpers exposed:**
```cpp
// From components/mod_fido2/include/mod_fido2/cbor_helpers.h:
typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t offset;
    bool error;
} cbor_writer_t;

void cbor_encode_uint(cbor_writer_t *w, uint64_t value);
// ... 30+ function prototypes for internal use
```

## Recommended Fix

1. **Consolidate into src/ directory**:
   ```
   components/mod_fido2/
   ├── include/mod_fido2/
   │   ├── Fido2Module.h     # Public API
   │   └── Fido2Ui.h         # UI (if needed externally)
   └── src/
       ├── Fido2Module.cpp
       ├── Fido2Ui.cpp
       ├── fido2.h           # Move here
       ├── fido2.cpp
       ├── fido2_common.h    # Move here
       ├── fido2_storage.h   # Move here
       ├── cbor_helpers.h    # Move here
       ├── cbor_helpers.cpp
       ├── ctaphid.h         # Move here
       ├── ctaphid.cpp
       ├── ctap2.h           # Move here
       ├── ctap2.cpp
       ├── pin_storage.h     # Move here
       └── u2f.h             # Move here
   ```

2. **Update internal includes**:
   ```cpp
   // In src/fido2.cpp:
   #include "fido2.h"
   #include "fido2_storage.h"
   #include "ctap2.h"
   #include "ctaphid.h"
   #include "fido2_common.h"  // Now internal
   ```

3. **Document public API**:
   - Add Doxygen group `@defgroup fido2-public Public API`
   - List only `Fido2Module` and `Fido2Ui` as public

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- FIDO2 specification: CTAP2 protocol internals should not be public
