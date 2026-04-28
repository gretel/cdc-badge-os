---
title: "[MEDIUM] TROPIC01 slot map is hardcoded in header file (requires manual edit before build)"
severity: MEDIUM
domain: extensibility
lens: architecture/extensibility
labels:
  - "audit:architecture/extensibility"
---

## Summary

The TROPIC01 secure element slot map (ECC and R-Memory allocation) is defined in a compile-time header file `main/tropic_slot_map.h` that must be manually edited before each build. This creates a rigid configuration that requires:

1. **Manual header editing** - Developers must edit the header file directly to add/modify slot allocations
2. **No runtime flexibility** - Slot allocation cannot be configured via NVS, serial commands, or other runtime mechanisms
3. **Build-time coupling** - The slot map is tightly coupled to the build, making it hard to support different hardware variants or deployment configurations

**Evidence:**

File: `main/tropic_slot_map.h` (lines 1-64)
```cpp
// Compile-time TROPIC01 slot map (edit before build)
//
// You have 32 ECC slots and 512 R-Memory slots.
// ...
// Syntax is always:
//   ECC_SLOT_<MODULENAME>_START / ECC_SLOT_<MODULENAME>_END
//   RMEM_SLOT_<MODULENAME>_START / RMEM_SLOT_<MODULENAME>_END

#define ECC_SLOT_MOD_GPG_START 1
#define ECC_SLOT_MOD_GPG_END 3
#define ECC_SLOT_MOD_CA_START 4
#define ECC_SLOT_MOD_CA_END 4
#define ECC_SLOT_MOD_FIDO2_START 5
#define ECC_SLOT_MOD_FIDO2_END 31

#define RMEM_SLOT_MOD_TOTP_START 32
#define RMEM_SLOT_MOD_TOTP_END 131
// ...
```

File: `components/cdc_core/src/TropicSlotMap.cpp` (lines 25-28)
```cpp
static const SlotMapEntry kSlotMap[] = {
    TROPIC_ECC_SLOT_MAP(BUILD_ECC_ENTRY)
    TROPIC_RMEM_SLOT_MAP(BUILD_RMEM_ENTRY)
};
```

The slot map is built using macro expansion at compile time, with no runtime configuration mechanism.

## Impact

- **Deployment inflexibility**: Different hardware variants or customer deployments cannot be configured without recompiling
- **Module addition friction**: Adding a new module requires editing the header, rebuilding, and potentially conflicting with existing allocations
- **No dynamic reallocation**: If a module needs more slots than allocated, there's no way to adjust without a full rebuild
- **Error-prone**: Manual editing of slot ranges can lead to overlaps or invalid configurations (even though validation exists)

## Recommended Fix

Implement a **configurable slot map** with the following layers:

1. **Default compile-time map** (current behavior, as fallback):
   - Keep `tropic_slot_map.h` as the default configuration
   - Used when no runtime config is available

2. **Runtime configuration via NVS**:
   - Allow slot map to be overridden via NVS storage
   - Provide serial commands to view/modify slot allocation
   - Store validated slot map in NVS for persistence

3. **Serial command interface** for slot management:
   ```
   SLOTMAP SHOW        # Display current slot map
   SLOTMAP ALLOC <mod> <type> <count>   # Allocate slots
   SLOTMAP VALIDATE    # Validate current map
   ```

**Scope for ~1 hour implementation:**
- Add NVS storage layer for slot map configuration
- Create `SlotMapConfig` struct with serialization support
- Implement `SLOTMAP SHOW` serial command to display current allocation

**Related work (split into separate issues):**
- Implement `SLOTMAP ALLOC` command for dynamic allocation
- Add validation and conflict detection for slot allocation
- Create UI for slot map management (专家 menu)

## References

- [`main/tropic_slot_map.h`](/input/20260423-132359-oj8ayc/cdc-badge-os/main/tropic_slot_map.h)
- [`components/cdc_core/src/TropicSlotMap.cpp`](/input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_core/src/TropicSlotMap.cpp)
- [TROPIC01 Documentation](https://www.tropic.co/) (external)
