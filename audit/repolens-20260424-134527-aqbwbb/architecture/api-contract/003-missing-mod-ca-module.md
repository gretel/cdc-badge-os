---
title: "[MEDIUM] Slot map defines mod_ca but no corresponding module exists"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The slot map configuration file (`main/tropic_slot_map.h`) defines ECC slot ranges for a module called `mod_ca`:

```cpp
#define MODULE_ID_MOD_CA 3
#define ECC_SLOT_MOD_CA_START 4
#define ECC_SLOT_MOD_CA_END 4

#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)
```

However, searching the codebase reveals no corresponding `mod_ca` module:
```bash
find components -name "*ca*" -o -name "*Ca*" | grep -v "\._"
# Returns: components/mod_gpg/src/ca.c (a single C file, not a full module)
```

This creates an API contract mismatch: the slot map expects a module named `mod_ca` to register itself, but no such module exists. If `mod_ca` is added later, there's no guarantee it will use the correct name.

## Impact
- **Wasted resources**: ECC slot 4 is reserved but unused
- **Configuration drift**: Slot map and actual modules are out of sync
- **Future bugs**: If someone adds a `mod_ca` module with a different name (e.g., `cert`), slot validation will fail
- **Confusion**: Developers may wonder if `mod_ca` is supposed to exist or is legacy

## Evidence
- Slot map definition: main/tropic_slot_map.h:32, 55
- No `mod_ca` module found: `find components/mod_* -name "*[Cc]a*" -type d` returns nothing
- Only `components/mod_gpg/src/ca.c` exists (certificate helper, not a full module)
- GPG module registers as "mod_gpg" (components/mod_gpg/include/mod_gpg/GpgModule.h:9)

## Recommended Fix
Choose one approach:

**Option A: Remove unused slot definition** (if `mod_ca` is not needed):
```cpp
// main/tropic_slot_map.h
#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)
```

**Option B: Create the missing module** (if `mod_ca` is planned):
1. Create `components/mod_ca/` directory structure
2. Implement `CaModule.h` and `CaModule.cpp` with `getName() { return "mod_ca"; }`
3. Add `mod_ca_register()` registration function
4. Add to `main/CMakeLists.txt` MODULES list

**Option C: Document as reserved** (if `mod_ca` is for future use):
```cpp
// main/tropic_slot_map.h
// NOTE: mod_ca reserved for certificate authority module (not yet implemented)
#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END) \
    // X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END)
```

## References
- Slot map: main/tropic_slot_map.h
- Module naming convention: components/cdc_core/include/cdc_core/IModule.h
- GPG module example: components/mod_gpg/include/mod_gpg/GpgModule.h
