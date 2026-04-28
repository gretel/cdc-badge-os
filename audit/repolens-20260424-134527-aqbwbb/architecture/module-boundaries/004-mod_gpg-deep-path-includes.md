---
title: "[LOW] mod_gpg uses deep path includes exposing internal directory structure"
severity: LOW
domain: architecture/module-boundaries
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module uses deep path includes like `#include "mod_gpg/openpgp/ccid.h"` that expose the internal directory structure. While this is internal to the module, it creates implicit coupling to the folder organization.

**Files:**
- `components/mod_gpg/src/ccid/ccid_driver.cpp:1-2`
- `components/mod_gpg/src/GpgModule.cpp:6`

**Evidence:**

In `components/mod_gpg/src/ccid/ccid_driver.cpp:1-2`:
```cpp
#include "mod_gpg/openpgp/ccid.h"
#include "mod_gpg/openpgp/openpgp.h"
```

In `components/mod_gpg/src/GpgModule.cpp:6`:
```cpp
#include "mod_gpg/openpgp/openpgp.h"
```

Compare to relative includes in same file:
```cpp
// In components/mod_gpg/src/openpgp/openpgp.cpp:9-14
#include "mod_gpg/openpgp/openpgp.h"  // Deep path
#include "mod_gpg/openpgp/apdu.h"     // Deep path
#include "ecdh.h"                      // Relative (better)
#include "pin_storage.h"               // Relative (better)
```

## Impact

1. **Fragile refactoring**: Moving `src/openpgp/` to `src/protocol/` requires updating all deep-path includes.
2. **Inconsistent style**: Mix of deep paths (`mod_gpg/openpgp/`) and relative includes (`ecdh.h`) in same codebase.
3. **Minor coupling**: Internal structure is visible in include statements.

## Recommended Fix

Standardize on relative includes for intra-module files:

1. **Update deep-path includes to relative**:
```cpp
// In src/ccid/ccid_driver.cpp
#include "../openpgp/ccid.h"      // Relative from src/ccid/
#include "../openpgp/openpgp.h"   // Relative from src/ccid/

// Or use src/ prefix for clarity
#include "openpgp/ccid.h"         // If src/ is in include path
#include "openpgp/openpgp.h"
```

2. **Keep consistent style** across all source files:
```cpp
// In src/openpgp/openpgp.cpp
#include "ecdh.h"        // Relative - already good
#include "pin_storage.h" // Relative - already good
#include "apdu.h"        // Relative - already good
```

3. **Update CMakeLists.txt** to ensure `src/` is in include path if using `openpgp/` prefix:
```cmake
INCLUDE_DIRS
    "include"
    "src"  # Allow #include "openpgp/openpgp.h" from src/
```

## References

- C++ include best practices: Prefer relative includes for intra-module files
- Module Architecture: Internal structure should be hidden from consumers
