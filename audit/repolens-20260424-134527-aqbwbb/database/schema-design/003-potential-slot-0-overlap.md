---
title: "[HIGH] Potential slot 0 overlap between PIN storage and module data"
severity: HIGH
domain: database/schema-design
lens: slot-allocation
labels:
  - "storage"
  - "slot-allocation"
  - "critical-data"
---

## Summary
The PIN storage uses R-Memory slot 0 (`PinManager.h:43`) as the primary PIN storage location. However, the module slot map in `tropic_slot_map.h` starts TOTP at slot 32 without explicitly reserving slots 1-31 for system use. If a module is reconfigured to use slot 0 or if the slot map is incorrectly updated, critical PIN data could be overwritten.

**Evidence:**
- `PinManager.h:43`: `static constexpr uint16_t RMEM_SLOT_PIN = 0;`
- `tropic_slot_map.h:46-51`:
  ```cpp
  #define RMEM_SLOT_MOD_TOTP_START 32
  #define RMEM_SLOT_MOD_TOTP_END 131
  #define RMEM_SLOT_MOD_FIDO2_START 132
  #define RMEM_SLOT_MOD_FIDO2_END 158
  #define RMEM_SLOT_MOD_PASSWORD_START 159
  #define RMEM_SLOT_MOD_PASSWORD_END 511
  ```
- `tropic_slot_map.h:28`: `static constexpr uint16_t RMEM_SLOT_MIN_ALLOC = 32;` - defines minimum but GPG uses slot 0 for DEC key

**Critical Finding**: `GpgStorage.cpp:45` shows GPG uses R-Memory slot 0 within its range for DEC key:
```cpp
static constexpr uint16_t RMEM_SLOT_DEC_KEY = 0;  // Relative to GPG range start
```

But GPG doesn't have an RMEM range defined in `tropic_slot_map.h`!

## Impact
1. **Data loss risk**: If GPG module is configured with RMEM range starting at 0, it would overlap with PIN storage
2. **Missing slot definition**: GPG uses R-Memory for DEC key but has no RMEM slot range in the map
3. **Silent corruption**: Without proper validation, writes to slot 0 could overwrite PIN data

## Evidence
From `tropic_slot_map.h`:
```cpp
// ECC slot ranges defined for GPG
#define ECC_SLOT_MOD_GPG_START 1
#define ECC_SLOT_MOD_GPG_END 3

// But NO RMEM range for GPG despite GpgStorage using R-Memory!
#define TROPIC_RMEM_SLOT_MAP(X) \
    X("mod_totp", MODULE_ID_MOD_TOTP, RMEM_SLOT_MOD_TOTP_START, RMEM_SLOT_MOD_TOTP_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, RMEM_SLOT_MOD_FIDO2_START, RMEM_SLOT_MOD_FIDO2_END) \
    X("mod_password", MODULE_ID_MOD_PASSWORD, RMEM_SLOT_MOD_PASSWORD_START, RMEM_SLOT_MOD_PASSWORD_END)
```

From `GpgStorage.cpp:42-47`:
```cpp
static constexpr uint16_t RMEM_SLOT_DEC_KEY = 0;
// ...
uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;
```

## Recommended Fix
1. **Add explicit RMEM slot range for GPG module** in `tropic_slot_map.h`
2. **Reserve slots 1-31** explicitly in the slot map for system use
3. **Add validation** in `TropicSlotMap::validateOnce()` to check for overlaps with critical system slots
4. **Document** the reserved slot regions clearly

Example fix for `tropic_slot_map.h`:
```cpp
// Reserved system slots
#define RMEM_SLOT_SYSTEM_START 1
#define RMEM_SLOT_SYSTEM_END 31

// GPG R-Memory (for encrypted DEC key)
#define RMEM_SLOT_MOD_GPG_START 33  // After reserved range
#define RMEM_SLOT_MOD_GPG_END 33    // Single slot for DEC key
```

## References
- TROPIC01 datasheet for slot organization
- OpenPGP card specification for key storage patterns
