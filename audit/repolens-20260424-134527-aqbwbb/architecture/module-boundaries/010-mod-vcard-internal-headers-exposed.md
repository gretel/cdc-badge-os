---
title: "[MEDIUM] mod_vcard: Internal BLE and storage headers exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_vcard` module exposes internal implementation headers in its public include directory:

- **`ble_vcard.h`** - BLE vCard exchange implementation (internal)
- **`vcard_store.h`** - vCard storage layer (internal)

These are implementation details, not part of the public API. Only `VcardModule.h` should be public.

## Impact

- **Implementation Leakage**: External modules can depend on internal BLE/storage logic
- **Tight Coupling**: Changes to BLE protocol or storage format break external consumers
- **Poor Encapsulation**: No clear distinction between public API and internal implementation
- **Refactoring Risk**: Moving internal files requires checking all external dependencies

## Evidence

**Current public include structure:**
```
components/mod_vcard/include/mod_vcard/
├── VcardModule.h     # Public API (correct)
├── ble_vcard.h       # BLE implementation (internal - exposed!)
└── vcard_store.h     # Storage layer (internal - exposed!)
```

**Internal header contents:**

`ble_vcard.h` (line 1-80):
```cpp
// BLE vCard Exchange state machine
typedef enum {
    VCARD_EXCHANGE_IDLE = 0,
    VCARD_EXCHANGE_CONNECTING,
    VCARD_EXCHANGE_DISCOVERING,
    // ... 8 more states
} vcard_exchange_state_t;

// BLE vCard peer structure
typedef struct {
    char name[VCARD_BLE_NAME_MAX];
    char slogan[VCARD_BLE_SLOGAN_MAX];
    int8_t rssi;
    uint8_t addr[6];
    // ...
} vcard_peer_t;

// BLE functions (internal implementation)
bool ble_vcard_init(void);
void ble_vcard_deinit(void);
bool ble_vcard_exchange_with(const uint8_t addr[6], uint8_t addr_type);
// ... 20+ BLE-specific functions
```

`vcard_store.h` (line 1-40):
```cpp
// vCard storage functions (internal)
bool vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len);
size_t vcard_store_get_own(char* out, size_t max_len);
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len);
bool vcard_store_delete(uint16_t slot);
// ... 8 more storage functions
```

**Usage in module:**
```cpp
// From components/mod_vcard/src/VcardModule.cpp:
#include "mod_vcard/ble_vcard.h"
#include "mod_vcard/vcard_store.h"

// From components/mod_vcard/src/ble_vcard.cpp:
#include "mod_vcard/ble_vcard.h"
#include "mod_vcard/vcard_store.h"
```

## Recommended Fix

1. **Move internal headers to src/**:
   ```
   components/mod_vcard/
   ├── include/mod_vcard/
   │   └── VcardModule.h     # Only public API
   └── src/
       ├── VcardModule.cpp
       ├── ble_vcard.h       # Move here (internal)
       ├── ble_vcard.cpp
       ├── vcard_store.h     # Move here (internal)
       └── vcard_store.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/VcardModule.cpp, change:
   #include "mod_vcard/ble_vcard.h"  # → #include "ble_vcard.h"
   #include "mod_vcard/vcard_store.h" # → #include "vcard_store.h"
   ```

3. **Add internal markers**:
   ```cpp
   // In src/ble_vcard.h:
   /**
    * @file ble_vcard.h
    * @brief Internal BLE vCard exchange - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `VcardModule.h`:
   ```cpp
   /**
    * @defgroup vcard-public Public API
    * @brief vCard module - BLE vCard exchange
    * 
    * Public classes:
    * - @ref VcardModule - Main module interface
    */
   ```

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- Similar pattern: `mod_password` exposes `PasswordStore.h` publicly (same issue)

(End of file - total 138 lines)
