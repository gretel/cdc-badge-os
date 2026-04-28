---
title: "[MEDIUM] mod_gpg pin_storage.h in include/ but implementation in src/"
severity: MEDIUM
domain: architecture/module-boundaries
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `pin_storage.h` header is located in `components/mod_gpg/include/` (making it publicly importable) while its implementation `pin_storage.cpp` is in `components/mod_gpg/src/`. This is a mismatch that suggests it's an internal detail exposed publicly.

**Files:**
- `components/mod_gpg/include/pin_storage.h` (public location)
- `components/mod_gpg/src/pin_storage.cpp` (implementation in src/)
- `components/mod_gpg/src/GpgModule.cpp:16` (uses `#include "pin_storage.h"`)

**Evidence:**

File structure:
```
components/mod_gpg/
├── include/
│   └── pin_storage.h      # Publicly importable
└── src/
    ├── GpgModule.cpp      # Line 16: #include "pin_storage.h"
    └── pin_storage.cpp    # Implementation
```

In `components/mod_gpg/src/GpgModule.cpp:16`:
```cpp
#include "pin_storage.h"
```

The header defines internal PIN storage functions:
```cpp
// components/mod_gpg/include/pin_storage.h:10-21
void pin_storage_openpgp_init(void);
bool pin_storage_openpgp_verify_pw1(const char *pin);
bool pin_storage_openpgp_verify_pw3(const char *pin);
bool pin_storage_openpgp_change_pw1(const char *new_pin);
bool pin_storage_openpgp_change_pw3(const char *new_pin);
uint8_t pin_storage_openpgp_pw1_retries(void);
// ... more internal functions
```

## Impact

1. **Internal API exposed**: PIN storage implementation details are publicly accessible to any module.
2. **Inconsistent with module pattern**: Other modules keep internal helpers in `src/`.
3. **Tight coupling risk**: External code could depend on these internal functions.
4. **Refactoring difficulty**: Moving or renaming these functions would break external consumers.

## Recommended Fix

Move `pin_storage.h` to `src/` directory:

1. **Move file**:
   ```bash
   mv components/mod_gpg/include/pin_storage.h components/mod_gpg/src/
   ```

2. **Update CMakeLists.txt** (if needed - currently no `INCLUDE_DIRS` change needed since it's not in the list):
   - Verify `pin_storage.h` is not in `INCLUDE_DIRS` (currently it's not)

3. **Update includes in source files**:
   ```cpp
   // In src/GpgModule.cpp:16
   #include "pin_storage.h"  // Still works - same directory as src/
   ```

4. **Verify no external modules include it**:
   - Search for `#include "mod_gpg/pin_storage.h"` in the codebase
   - If found, either:
     a. Move it to `include/mod_gpg/` with proper namespace, OR
     b. Export via `GpgModule.h` as needed

## References

- Module structure convention: `components/<module>/include/` for public, `src/` for internal
- `components/mod_fido2/` - similar module with `pin_storage.h` in `include/mod_fido2/` (same issue)
