---
title: "[MEDIUM] mod_fido2 exposes internal implementation headers in public include path"
severity: MEDIUM
domain: architecture/module-boundaries
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_fido2` module exposes all headers in `include/mod_fido2/` publicly, including internal implementation details like `cbor_helpers.h`, `ctaphid.h`, `fido2_storage.h`, `ctap2.h`, `u2f.h` that should be internal to the module.

**Files:**
- `components/mod_fido2/include/mod_fido2/cbor_helpers.h` (internal helper)
- `components/mod_fido2/include/mod_fido2/ctaphid.h` (internal transport)
- `components/mod_fido2/include/mod_fido2/fido2_storage.h` (internal storage)
- `components/mod_fido2/src/Fido2Module.cpp:6-8` (internal includes)

**Evidence:**

All these headers are in `include/mod_fido2/` (public path):
```
components/mod_fido2/include/mod_fido2/
├── Fido2Module.h        # Public API
├── Fido2Ui.h            # UI layer
├── cbor_helpers.h       # Internal CBOR encoding
├── ctaphid.h            # Internal CTAPHID transport
├── fido2_storage.h      # Internal storage layer
├── ctap2.h              # Internal CTAP2 protocol
├── u2f.h                # Internal U2F protocol
├── fido2.h              # Core protocol
├── fido2_common.h       # Common types
└── pin_storage.h        # Internal PIN storage
```

In `components/mod_fido2/src/Fido2Module.cpp:6-8`:
```cpp
#include "mod_fido2/fido2.h"
#include "mod_fido2/fido2_storage.h"  // Internal
#include "mod_fido2/ctaphid.h"        // Internal
```

`cbor_helpers.h` is clearly a utility:
```cpp
// components/mod_fido2/include/mod_fido2/cbor_helpers.h:1-2
// CBOR Encoding/Decoding Helpers for CTAP2
// Minimal CBOR implementation for FIDO2
```

## Impact

1. **API surface bloat**: Internal implementation details (CBOR helpers, CTAPHID framing) are publicly accessible.
2. **Unnecessary coupling**: External modules could depend on internal protocols.
3. **Refactoring difficulty**: Changing internal structures breaks external consumers.
4. **Confusion**: Developers can't tell what's stable public API vs. internal.

## Recommended Fix

Identify and separate public vs. internal headers:

1. **Public API** (keep in `include/mod_fido2/`):
   - `Fido2Module.h` - Main module class
   - `Fido2Ui.h` - UI interface (if needed externally)

2. **Internal headers** (move to `src/`):
   - `cbor_helpers.h` → `src/cbor_helpers.h`
   - `ctaphid.h` → `src/ctaphid.h`
   - `fido2_storage.h` → `src/fido2_storage.h`
   - `ctap2.h` → `src/ctap2.h`
   - `u2f.h` → `src/u2f.h`
   - `pin_storage.h` → `src/pin_storage.h`

3. **Update includes in source files**:
```cpp
// In src/Fido2Module.cpp
#include "mod_fido2/Fido2Module.h"
#include "mod_fido2/Fido2Ui.h"
#include "fido2.h"         // Relative include from src/
#include "fido2_storage.h" // Relative include from src/
#include "ctaphid.h"       // Relative include from src/
```

4. **Update CMakeLists.txt**:
```cmake
INCLUDE_DIRS
    "include"
    # Internal headers use relative includes from src/
```

## References

- Module Architecture documentation: `components/cdc_core/IModule.h`
- `components/mod_gpg/` - similar structure but with better separation (though still has issues)
