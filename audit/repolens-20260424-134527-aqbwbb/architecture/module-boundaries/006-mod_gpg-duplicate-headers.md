---
title: "[MEDIUM] mod_gpg has duplicate header files in include/ and src/"
severity: MEDIUM
domain: architecture/module-boundaries
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module has duplicate header files - some headers exist in both `include/mod_gpg/openpgp/` AND `src/openpgp/`. This creates confusion about which header is the "official" one and can lead to inconsistencies.

**Files:**
- `components/mod_gpg/include/mod_gpg/openpgp/` (public headers)
- `components/mod_gpg/src/openpgp/` (internal headers)

**Evidence:**

Headers in `include/mod_gpg/openpgp/`:
```
include/mod_gpg/openpgp/
├── apdu.h
├── ccid.h
└── openpgp.h
```

Headers in `src/openpgp/`:
```
src/openpgp/
├── apdu.cpp
├── ccid.cpp
├── ecdh.cpp
├── ecdh.h      # Only in src/
└── openpgp.cpp
```

**Current include behavior:**
```cpp
// In src/GpgModule.cpp:6
#include "mod_gpg/openpgp/openpgp.h"  // Resolves to include/mod_gpg/openpgp/openpgp.h

// In src/openpgp/openpgp.cpp:9
#include "mod_gpg/openpgp/openpgp.h"  // Same file

// In src/openpgp/openpgp.cpp:13
#include "ecdh.h"  // Resolves to src/openpgp/ecdh.h (via "src/openpgp" in INCLUDE_DIRS)
```

**CMakeLists.txt configuration:**
```cmake
INCLUDE_DIRS
    "include"
    "src/openpgp"  # Both paths are exposed!
```

## Impact

1. **Confusion**: Developers don't know which header to include.
2. **Potential inconsistency**: If headers diverge, different code uses different versions.
3. **Build reliability**: Depends on CMake include path order to resolve correctly.
4. **Maintenance burden**: Changes may need to be applied to both locations.

## Recommended Fix

Consolidate to a single location:

**Option A: Keep in include/ (if truly public)**
```
include/mod_gpg/openpgp/
├── apdu.h
├── ccid.h
├── ecdh.h      # Move from src/
└── openpgp.h
```

Update CMakeLists.txt:
```cmake
INCLUDE_DIRS
    "include"
    # Remove "src/openpgp" - all headers now in include/
```

Update includes:
```cpp
// In src/openpgp/openpgp.cpp
#include "mod_gpg/openpgp/openpgp.h"
#include "mod_gpg/openpgp/ecdh.h"  # Use full path
```

**Option B: Move all to src/ (if internal)**
```
src/openpgp/
├── apdu.h      # Move from include/
├── apdu.cpp
├── ccid.h      # Move from include/
├── ccid.cpp
├── ecdh.h
├── ecdh.cpp
├── openpgp.h   # Move from include/
└── openpgp.cpp
```

Update CMakeLists.txt:
```cmake
INCLUDE_DIRS
    "include"
    "src/openpgp"  # Keep for internal includes
```

Update includes:
```cpp
// In src/openpgp/openpgp.cpp
#include "openpgp.h"  # Relative include
#include "ecdh.h"     # Relative include
```

**Recommendation**: Since `openpgp.h`, `apdu.h`, `ccid.h` are used by `src/GpgModule.cpp` (which is in a different directory), Option A is probably correct - move `ecdh.h` to `include/mod_gpg/openpgp/` and remove `src/openpgp` from `INCLUDE_DIRS`.

## References

- Module Architecture: Headers should be in one canonical location
- CMake: `INCLUDE_DIRS` should be minimal and unambiguous
