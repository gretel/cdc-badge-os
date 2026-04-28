---
title: "[MEDIUM] mod_fido2: Internal CTAP/U2F protocol headers exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_fido2` module exposes low-level CTAP (Common Transport Application Protocol) and U2F protocol headers in its public include directory, allowing external consumers to reach into internal protocol implementation:

- **`ctaphid.h`** - CTAP HID transport layer (internal)
- **`ctap2.h`** - CTAP2 protocol handler (internal)
- **`u2f.h`** - U2F/CTAP1 legacy protocol (internal)

These are implementation details of the FIDO2 module, not part of its public API.

## Impact

- **Implementation Leakage**: External modules can depend on internal protocol handlers
- **Tight Coupling**: Changes to CTAP/U2F internals break external consumers
- **Namespace Pollution**: Internal types and constants clutter the public API
- **Protocol Encapsulation**: CTAP HID transport should be hidden from consumers

## Evidence

**Current public include structure:**
```
components/mod_fido2/include/mod_fido2/
├── Fido2Module.h         # Public API (correct)
├── Fido2Ui.h             # UI layer (maybe public?)
├── fido2.h               # Core FIDO2 (internal?)
├── fido2_common.h        # Internal helpers
├── fido2_storage.h       # Storage (internal)
├── cbor_helpers.h        # CBOR encoding (internal)
├── ctaphid.h             # CTAP HID (internal - exposed!)
├── ctap2.h               # CTAP2 protocol (internal - exposed!)
├── fido2_common.h        # Common helpers
├── pin_storage.h         # PIN storage (internal)
└── u2f.h                 # U2F protocol (internal - exposed!)
```

**Internal protocol headers exposed:**

`ctaphid.h` (line 1-40):
```cpp
// CTAP HID transport layer
#define CTAPHID_PING        0x01
#define CTAPHID_MSG         0x03
#define CTAPHID_INIT        0x06
#define CTAPHID_WINK        0x08
#define CTAPHID_SEND_RNG    0x0A
// ... 20+ HID command definitions
```

`ctap2.h` (line 1-60):
```cpp
// CTAP2 protocol handler
#define CTAP2_CMD_MAKE_CREDENTIAL       0x01
#define CTAP2_CMD_GET_ASSERTION         0x02
#define CTAP2_CMD_GET_INFO              0x04
// ... 30+ status codes, algorithm definitions
```

`u2f.h` (line 1-40):
```cpp
// U2F/CTAP1 legacy protocol
#define U2F_INS_REGISTER        0x01
#define U2F_INS_AUTHENTICATE    0x02
#define U2F_INS_VERSION         0x03
// ... U2F constants and handlers
```

**Usage in module:**
```cpp
// From components/mod_fido2/src/Fido2Module.cpp:
#include "mod_fido2/ctaphid.h"
#include "mod_fido2/ctap2.h"
#include "mod_fido2/u2f.h"
```

## Recommended Fix

1. **Move protocol headers to src/**:
   ```
   components/mod_fido2/
   ├── include/mod_fido2/
   │   ├── Fido2Module.h     # Public API
   │   └── Fido2Ui.h         # UI layer (if needed externally)
   └── src/
       ├── Fido2Module.cpp
       ├── Fido2Ui.cpp
       ├── fido2.h
       ├── fido2.cpp
       ├── fido2_common.h
       ├── fido2_storage.h
       ├── cbor_helpers.h
       ├── cbor_helpers.cpp
       ├── ctaphid.h         # Move here (internal)
       ├── ctaphid.cpp
       ├── ctap2.h           # Move here (internal)
       ├── ctap2.cpp
       ├── u2f.h             # Move here (internal)
       ├── u2f.cpp
       └── pin_storage.h
   ```

2. **Update internal includes**:
   ```cpp
   // In src/Fido2Module.cpp, change:
   #include "mod_fido2/ctaphid.h"  # → #include "ctaphid.h"
   #include "mod_fido2/ctap2.h"    # → #include "ctap2.h"
   #include "mod_fido2/u2f.h"      # → #include "u2f.h"
   ```

3. **Add internal markers**:
   ```cpp
   // In src/ctaphid.h:
   /**
    * @file ctaphid.h
    * @brief Internal CTAP HID transport - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `Fido2Module.h`:
   ```cpp
   /**
    * @defgroup fido2-public Public API
    * @brief FIDO2/WebAuthn module
    * 
    * Public classes:
    * - @ref Fido2Module - Main module interface
    * - @ref Fido2Ui     - UI integration
    */
   ```

## References

- FIDO2 specification: CTAP2 protocol is internal implementation detail
- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`

(End of file - total 156 lines)
