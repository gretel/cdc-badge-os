---
title: "[HIGH] Module structure inconsistency: pin_storage.h in wrong location"
severity: HIGH
domain: code-quality/consistency
lens: module-structure
labels:
  - "audit:code-quality/consistency"
---

## Summary
The `mod_gpg` module has `pin_storage.h` in the wrong directory location, breaking the established module structure pattern.

**Current (incorrect) location**:
```
components/mod_gpg/include/pin_storage.h  // Wrong!
```

**Expected location** (consistent with other modules):
```
components/mod_gpg/include/mod_gpg/pin_storage.h  // Correct
```

**Evidence of inconsistency**:
- All other module headers are in `include/<module_name>/` subdirectory
- `mod_gpg` has most headers correctly in `include/mod_gpg/`:
  - `GpgModule.h`
  - `GpgStorage.h`
  - `gpg.h`
  - `openpgp/apdu.h`
  - `openpgp/ccid.h`
  - `openpgp/openpgp.h`
- But `pin_storage.h` is directly in `include/`

**CMakeLists.txt workaround**:
```cmake
INCLUDE_DIRS
    "include"
    "src/openpgp"
    "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
```
The `"include"` path is needed because `pin_storage.h` is incorrectly placed there.

## Impact
- **Confusing module structure**: Violates the established convention
- **Inconsistent include paths**: Requires `"pin_storage.h"` instead of `"mod_gpg/pin_storage.h"`
- **Fragile build configuration**: CMakeLists.txt needs extra include paths
- **Maintenance friction**: New developers may place files incorrectly

## Evidence
**Current file structure**:
```
components/mod_gpg/include/
├── pin_storage.h          // Wrong location
└── mod_gpg/
    ├── GpgModule.h
    ├── GpgStorage.h
    ├── gpg.h
    └── openpgp/
        ├── apdu.h
        ├── ccid.h
        └── openpgp.h
```

**Include in source files**:
```cpp
// components/mod_gpg/src/GpgModule.cpp
#include "pin_storage.h"  // Works because include/ is in path

// Should be:
#include "mod_gpg/pin_storage.h"
```

**Comparison with correct module structure** (mod_totp):
```
components/mod_totp/include/mod_totp/
├── TotpModule.h
└── TotpStore.h
```

## Recommended Fix
1. **Move file**:
   ```bash
   mv components/mod_gpg/include/pin_storage.h components/mod_gpg/include/mod_gpg/
   mv components/mod_gpg/src/pin_storage.cpp components/mod_gpg/src/  # Already correct
   ```

2. **Update includes** in source files:
   ```cpp
   // components/mod_gpg/src/GpgModule.cpp
   // Change:
   #include "pin_storage.h"
   // To:
   #include "mod_gpg/pin_storage.h"
   ```

3. **Update CMakeLists.txt** (optional cleanup):
   ```cmake
   INCLUDE_DIRS
       "include"
       "src/openpgp"
   # No change needed - "include" still works, but path is more logical now
   ```

4. **Update header guard** (if using traditional guards):
   ```cpp
   // Rename guard from PIN_STORAGE_H to MOD_GPG_PIN_STORAGE_H
   ```

## References
- Module structure pattern established in `mod_totp`, `mod_password`, `mod_fido2`
- CMakeLists.txt in `main/` expects consistent module structure
- All other modules follow `include/<module_name>/` pattern
