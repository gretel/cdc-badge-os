---
title: "[MEDIUM] mod_sao: Internal C-style header exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_sao` module exposes an internal C-style header (`sao.h`) in its public include directory:

- **`sao.h`** - SAO (Standardized Add-On) parser implementation (internal)

Only `SaoModule.h` should be public. The `sao.h` header contains low-level implementation details of the SAO binary descriptor parser.

## Impact

- **Implementation Leakage**: External modules can depend on internal SAO parser functions
- **Poor Encapsulation**: C-style API mixed with C++ module class
- **Namespace Pollution**: Internal types like `sao_descriptor_t`, `sao_driver_info_t` exposed publicly
- **Refactoring Risk**: Changes to SAO parser internals break external consumers

## Evidence

**Current public include structure:**
```
components/mod_sao/include/mod_sao/
├── SaoModule.h     # Public API (correct)
└── sao.h           # SAO parser (internal - exposed!)
```

**Internal header contents:**

`sao.h` (line 1-80):
```cpp
// SAO Binary Descriptor Parser
// Implements the badge.team SAO standard

// SAO Driver Information
typedef struct {
    char name[SAO_MAX_DRIVER_NAME_LEN + 1];
    uint8_t data[SAO_MAX_DRIVER_DATA_LEN];
    uint8_t data_len;
} sao_driver_info_t;

// SAO Descriptor (parsed from EEPROM)
typedef struct {
    char name[SAO_MAX_NAME_LEN + 1];
    sao_driver_info_t primary_driver;
    sao_driver_info_t extra_drivers[SAO_MAX_EXTRA_DRIVERS];
    uint8_t extra_driver_count;
} sao_descriptor_t;

// SAO functions (internal implementation)
bool sao_init(void);
bool sao_scan(void);
bool sao_read_descriptor(sao_descriptor_t* desc);
bool sao_is_detected(void);
const char* sao_get_name(void);
const char* sao_get_driver_name(void);
void sao_get_info_string(char* buf, size_t len);
```

**Usage in module:**
```cpp
// From components/mod_sao/src/SaoModule.cpp:
#include "mod_sao/sao.h"
```

**SAO constants exposed:**
```cpp
#define SAO_EEPROM_ADDR         0x50
#define SAO_MAX_NAME_LEN        63
#define SAO_MAX_DRIVER_NAME_LEN 31
#define SAO_MAX_DRIVER_DATA_LEN 64
#define SAO_MAX_EXTRA_DRIVERS   4
```

## Recommended Fix

1. **Move sao.h to src/**:
   ```
   components/mod_sao/
   ├── include/mod_sao/
   │   └── SaoModule.h     # Only public API
   └── src/
       ├── SaoModule.cpp
       └── sao.h           # Move here (internal)
           sao.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/SaoModule.cpp, change:
   #include "mod_sao/sao.h"  # → #include "sao.h"
   ```

3. **Add internal marker**:
   ```cpp
   // In src/sao.h:
   /**
    * @file sao.h
    * @brief Internal SAO parser - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `SaoModule.h`:
   ```cpp
   /**
    * @defgroup sao-public Public API
    * @brief SAO module - Standardized Add-On detection
    * 
    * Public classes:
    * - @ref SaoModule - Main module interface
    */
   ```

5. **Consider public interface**:
   - If external modules need SAO info, expose via `SaoModule` methods
   - Keep `sao_descriptor_t` and related types internal

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- SAO specification: https://badge.team/docs/standards/sao/binary_descriptor/

(End of file - total 138 lines)
