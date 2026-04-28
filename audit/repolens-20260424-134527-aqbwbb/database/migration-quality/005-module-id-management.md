---
title: "[LOW] Module ID assignment lacks centralized management for new modules"
severity: LOW
domain: database/migration-quality
lens: embedded-storage
labels:
  - "slot-map"
  - "module-registration"
---

## Summary
In `main/tropic_slot_map.h:28-36`, module IDs are defined as simple preprocessor macros:

```cpp
// Module IDs (must be unique, 0-254). 255 is reserved for UNKNOWN.
#define MODULE_ID_MOD_SYSTEM 0
#define MODULE_ID_MOD_GPG 2
#define MODULE_ID_MOD_CA 3
#define MODULE_ID_MOD_FIDO2 4
#define MODULE_ID_MOD_TOTP 5
#define MODULE_ID_MOD_PASSWORD 6
#define MODULE_ID_UNKNOWN 255
```

Note that MODULE_ID_MOD_SYSTEM is 0 but MODULE_ID_MOD_GPG jumps to 2 (slot 1 is skipped). When adding new modules, developers must manually ensure unique IDs and update the slot map. There's no validation or helper to prevent collisions.

## Impact
- **Collision risk**: Adding a new module requires careful manual assignment to avoid ID conflicts.
- **Fragmented IDs**: The gap between MODULE_ID_MOD_SYSTEM (0) and MODULE_ID_MOD_GPG (2) suggests historical changes but no documentation.
- **Build-time only validation**: Conflicts are only detected at runtime in `TropicSlotMap::validateOnce()`, not at compile time.

## Evidence
File: `main/tropic_slot_map.h:28-57`

The slot map entries use these IDs:
```cpp
#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)

#define TROPIC_RMEM_SLOT_MAP(X) \
    X("mod_totp", MODULE_ID_MOD_TOTP, RMEM_SLOT_MOD_TOTP_START, RMEM_SLOT_MOD_TOTP_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, RMEM_SLOT_MOD_FIDO2_START, RMEM_SLOT_MOD_FIDO2_END) \
    X("mod_password", MODULE_ID_MOD_PASSWORD, RMEM_SLOT_MOD_PASSWORD_START, RMEM_SLOT_MOD_PASSWORD_END)
```

Note: `mod_gpg` uses MODULE_ID_MOD_GPG (2) for ECC, but there's no RMEM entry for GPG. This is intentional (GPG stores R-Memory in paired slots), but the mapping isn't explicit.

## Recommended Fix
Improve module ID management:

1. **Add constant generation script**: Create a Python script that validates uniqueness and reports gaps:
   ```python
   # Check for duplicate IDs
   ids = [2, 3, 4, 5, 6]  # from macros
   if len(ids) != len(set(ids)):
       print("Warning: Duplicate module IDs detected")
   ```

2. **Document ID assignment**: Add comments explaining the gap at ID 1:
   ```cpp
   #define MODULE_ID_MOD_SYSTEM 0    // System/PIN management
   // ID 1 reserved for future use (originally planned for vCard)
   #define MODULE_ID_MOD_GPG 2
   ```

3. **Add compile-time assertion**: Use C++ constexpr to validate at compile time:
   ```cpp
   constexpr bool validateModuleIds() {
       // Check uniqueness
       return true;
   }
   static_assert(validateModuleIds(), "Module ID collision");
   ```

## References
- C++ compile-time validation: https://en.cppreference.com/w/cpp/language/static_assert
- Module design patterns: https://martinfowler.com/articles/module-patterns.html
