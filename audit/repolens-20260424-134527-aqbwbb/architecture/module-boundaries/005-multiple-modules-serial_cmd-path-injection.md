---
title: "[LOW] Multiple modules inject serial_cmd/include path creating implicit coupling"
severity: LOW
domain: architecture/module-boundaries
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

Multiple modules (`mod_gpg`, `mod_password`, `mod_totp`, `mod_vcard`) inject `${CMAKE_SOURCE_DIR}/components/serial_cmd/include` into their `INCLUDE_DIRS`, creating implicit coupling to the `serial_cmd` module's internal structure.

**Files:**
- `components/mod_gpg/CMakeLists.txt:17`
- `components/mod_password/CMakeLists.txt`
- `components/mod_totp/CMakeLists.txt`
- `components/mod_vcard/CMakeLists.txt`

**Evidence:**

In `components/mod_gpg/CMakeLists.txt:17`:
```cmake
INCLUDE_DIRS
    "include"
    "src/openpgp"
    "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"  # Path injection
```

Same pattern in `mod_password`, `mod_totp`, `mod_vcard`:
```cmake
INCLUDE_DIRS
    "include"
    "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
```

Usage in `components/mod_gpg/src/GpgModule.cpp:17`:
```cpp
#include "serial_cmd/ICommandRegistry.h"
#include "serial_cmd/Console.h"
```

## Impact

1. **Implicit dependency**: `serial_cmd` is in `REQUIRES` but path injection exposes its internal structure.
2. **Fragile coupling**: If `serial_cmd` reorganizes its include path, all dependent modules break.
3. **Redundant declaration**: `serial_cmd` is already in `REQUIRES`, so CMake should handle include paths.
4. **Inconsistent with build system**: Why manually inject paths when `REQUIRES` should handle it?

## Recommended Fix

Remove manual path injection and rely on CMake's dependency resolution:

1. **Update CMakeLists.txt** (for each module):
```cmake
idf_component_register(
    SRCS
        # ... source files
    INCLUDE_DIRS
        "include"
        # Remove: "${CMAKE_SOURCE_DIR}/components/serial_cmd/include"
    REQUIRES
        cdc_core
        cdc_ui
        # ... other deps
        serial_cmd  # Already here - should handle includes
)
```

2. **Verify includes still work**:
```cpp
// In src/GpgModule.cpp
#include "serial_cmd/ICommandRegistry.h"  // Should still work via REQUIRES
#include "serial_cmd/Console.h"
```

3. **If includes break**, check `serial_cmd/CMakeLists.txt` to ensure it exposes the right paths:
```cmake
# components/serial_cmd/CMakeLists.txt
idf_component_register(
    INCLUDE_DIRS "include"  # Should expose serial_cmd/
    # ...
)
```

## References

- ESP-IDF component registration: `REQUIRES` should handle transitive include paths
- Module Architecture: Dependencies should be declared, not manually wired
