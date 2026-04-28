---
title: "[MEDIUM] mod_gpg: Internal openpgp/ subdirectory headers accessible via public include path"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module exposes its internal `openpgp/` subdirectory in the public include path, allowing external consumers to reach into low-level protocol implementation details:

- **`components/mod_gpg/include/mod_gpg/openpgp/`** contains internal protocol headers:
  - `openpgp.h` - Core OpenPGP application logic
  - `apdu.h` - ISO 7816 APDU parser
  - `ccid.h` - CCID protocol definitions

These headers are accessed via paths like `#include "mod_gpg/openpgp/openpgp.h"` which means:
1. The folder structure is part of the public API contract
2. External modules could theoretically import these internal headers
3. Moving or renaming these files would break any external consumers

## Impact

- **Implementation Leakage**: Low-level protocol headers (APDU, CCID) are exposed publicly when they should be internal
- **Brittle Dependencies**: External code could depend on internal files that may move or change
- **Refactoring Blocked**: Moving `openpgp/` to `src/` requires checking all external consumers
- **Poor Encapsulation**: No clear distinction between public API (`GpgModule.h`) and internal implementation

## Evidence

**Current public include structure:**
```
components/mod_gpg/include/
└── mod_gpg/
    ├── GpgModule.h       # Public API (correct)
    ├── GpgStorage.h      # Storage layer (borderline)
    ├── gpg.h             # Internal backend (exposed)
    └── openpgp/          # INTERNAL - exposed publicly!
        ├── openpgp.h     # OpenPGP app (internal)
        ├── apdu.h        # APDU parser (internal)
        └── ccid.h        # CCID protocol (internal)
```

**Include pattern showing exposure:**
```cpp
// From components/mod_gpg/src/GpgModule.cpp:
#include "mod_gpg/openpgp/openpgp.h"  // Internal header via public path

// From components/mod_gpg/src/gpg.cpp:
#include "mod_gpg/openpgp/openpgp.h"

// From components/mod_gpg/src/ccid/ccid_driver.cpp:
#include "mod_gpg/openpgp/openpgp.h"
```

**Internal headers with public accessibility:**
- `apdu.h` - ISO 7816 APDU parser (line 1-60): Defines `apdu_t` structure and parsing functions
- `openpgp.h` - OpenPGP application (line 1-80): Defines APDU instructions, DO tags, status words
- `ccid.h` - CCID protocol (in openpgp/ subdirectory)

## Recommended Fix

1. **Move openpgp/ subdirectory to src/**:
   ```
   components/mod_gpg/
   ├── include/mod_gpg/
   │   ├── GpgModule.h      # Only public API
   │   └── GpgStorage.h     # Storage layer
   └── src/
       ├── openpgp/         # Move here (internal)
       │   ├── openpgp.h
       │   ├── openpgp.cpp
       │   ├── apdu.h
       │   └── ccid.h
       └── ccid/
           └── ccid_driver.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/GpgModule.cpp, change:
   #include "mod_gpg/openpgp/openpgp.h"  // → #include "openpgp/openpgp.h"
   
   // In src/gpg.cpp, change:
   #include "mod_gpg/openpgp/openpgp.h"  // → #include "openpgp/openpgp.h"
   ```

3. **Update CMakeLists.txt**:
   ```cmake
   # Ensure only include/mod_gpg/ is in public INCLUDE_DIRS
   # src/openpgp/ is compiled but not exposed externally
   ```

4. **Add internal marker to moved headers**:
   ```cpp
   // components/mod_gpg/src/openpgp/openpgp.h
   /**
    * @file openpgp.h
    * @brief Internal OpenPGP implementation - NOT part of public API
    */
   ```

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- Similar pattern in `mod_fido2`: Internal `ctaphid.h`, `ctap2.h` should also be moved to src/

(End of file - total 136 lines)
