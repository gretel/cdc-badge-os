---
title: "[MEDIUM] mod_gpg: Internal subdirectory structure exposed via public headers"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module exposes its internal subdirectory structure (`openpgp/`, `ccid/`) in the public include path, allowing external consumers to reach into implementation details:

- `components/mod_gpg/include/mod_gpg/openpgp/` contains low-level OpenPGP protocol headers
- `components/mod_gpg/include/mod_gpg/ccid/` is an empty directory (cleanup needed)
- These subdirectories are referenced by includes like `#include "mod_gpg/openpgp/openpgp.h"`

## Impact

- **Implementation Leakage**: The module's folder structure becomes part of the public API contract
- **Brittle Dependencies**: External code can depend on internal files that may move or change
- **Refactoring Blocked**: Moving internal files requires updating all external consumers
- **Poor Encapsulation**: No clear distinction between public API and internal implementation

## Evidence

**Current public include structure:**
```
components/mod_gpg/include/
└── mod_gpg/
    ├── GpgModule.h       # Public API
    ├── GpgStorage.h      # Storage layer
    ├── gpg.h             # Internal backend
    ├── openpgp/          # INTERNAL - exposed publicly!
    │   ├── apdu.h        # APDU protocol (internal)
    │   ├── ccid.h        # CCID driver (internal)
    │   └── openpgp.h     # OpenPGP implementation (internal)
    └── ccid/             # Empty directory
```

**Include pattern showing exposure:**
```cpp
// From components/mod_gpg/src/GpgModule.cpp:
#include "mod_gpg/openpgp/openpgp.h"  // Internal header via public path
```

**External modules could theoretically include:**
```cpp
#include "mod_gpg/openpgp/apdu.h"    // Internal APDU protocol
#include "mod_gpg/openpgp/ccid.h"    // Internal CCID driver
#include "mod_gpg/gpg.h"             // Internal backend
```

## Recommended Fix

1. **Consolidate internal headers into src/**:
   ```
   components/mod_gpg/
   ├── include/
   │   └── mod_gpg/
   │       ├── GpgModule.h
   │       └── GpgStorage.h
   └── src/
       └── openpgp/
           ├── openpgp.h       # Move from include/
           ├── openpgp.cpp
           ├── apdu.h          # Move from include/
           ├── apdu.cpp
           ├── ccid.h          # Move from include/
           ├── ccid.cpp
           ├── gpg.h           # Move from include/
           ├── gpg.cpp
           └── ecdh.h          # Already in src/
   ```

2. **Update internal includes**:
   ```cpp
   // In src/GpgModule.cpp:
   #include "openpgp/openpgp.h"  // Relative to src/
   ```

3. **Remove empty directory**:
   ```bash
   rmdir components/mod_gpg/include/mod_gpg/ccid/
   ```

4. **Document public API**:
   - Add comment block to `GpgModule.h` listing the public interface
   - Add `/// @defgroup gpg-public Public API` Doxygen group

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules must be: Removable, Isolated, Self-registering"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
