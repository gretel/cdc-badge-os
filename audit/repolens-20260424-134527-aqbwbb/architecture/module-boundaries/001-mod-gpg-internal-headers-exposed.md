---
title: "[HIGH] mod_gpg exposes internal src/ directory headers via CMake INCLUDE_DIRS"
severity: HIGH
domain: architecture/module-boundaries
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module exposes its internal `src/openpgp/` directory in the CMake `INCLUDE_DIRS`, allowing external consumers to include internal implementation headers directly. This breaks module encapsulation.

**Files:**
- `components/mod_gpg/CMakeLists.txt:16`
- `components/mod_gpg/src/ccid/ccid_driver.cpp:1-2`
- `components/mod_gpg/src/openpgp/openpgp.cpp:9-14`

**Evidence:**

In `components/mod_gpg/CMakeLists.txt`:
```cmake
INCLUDE_DIRS
    "include"
    "src/openpgp"  # Internal src/ directory exposed!
```

This allows includes like:
```cpp
// In src/ccid/ccid_driver.cpp:1-2
#include "mod_gpg/openpgp/ccid.h"
#include "mod_gpg/openpgp/openpgp.h"

// In src/openpgp/openpgp.cpp:9-14
#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/apdu.h"
#include "ecdh.h"  // Relative include from src/openpgp/
```

## Impact

1. **Encapsulation violation**: External modules can import internal headers like `ecdh.h`, `apdu.h`, `ccid.h` that are implementation details of the OpenPGP stack.
2. **Tight coupling**: Consumers become dependent on internal directory structure (`src/openpgp/`), making refactoring harder.
3. **API surface bloat**: Internal types and functions become part of the "public" API, even though they're not intended for external use.
4. **Maintenance burden**: Changes to internal headers may break external code that shouldn't have been using them.

## Recommended Fix

Move internal headers to a location that is NOT in the `INCLUDE_DIRS`:

1. **Keep only public API in `include/mod_gpg/`**:
   - `GpgModule.h` - Main module header
   - `GpgStorage.h` - Public storage API

2. **Move internal headers**:
   - `src/openpgp/` → keep as-is (already in `src/`, just don't add to `INCLUDE_DIRS`)
   - `ecdh.h`, `apdu.h`, `ccid.h`, `openpgp.h` should only be included via relative paths from within `src/`

3. **Update CMakeLists.txt**:
```cmake
INCLUDE_DIRS
    "include"
    # Remove "src/openpgp" - internal headers use relative includes
    "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
```

4. **Update internal includes** (already using relative includes, just verify):
```cpp
// In src/openpgp/openpgp.cpp
#include "ecdh.h"        // Already uses relative include ✓
#include "pin_storage.h" // Already uses relative include ✓
```

## References

- Module Architecture documentation: `components/cdc_core/IModule.h`
- CMake `idf_component_register()` documentation: `INCLUDE_DIRS` should expose public API only
- Clean Architecture principles: Internal implementation details should not be visible outside the module
