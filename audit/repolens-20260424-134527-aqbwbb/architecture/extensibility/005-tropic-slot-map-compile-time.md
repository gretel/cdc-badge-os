---
title: "[MEDIUM] TROPIC Slot Map is Compile-Time Fixed"
severity: MEDIUM
domain: extensibility
lens: memory-allocation
labels:
  - "audit:architecture/extensibility"
---

## Summary
The TROPIC01 slot map in `main/tropic_slot_map.h` is a compile-time configuration that requires editing the header and rebuilding for any module to get memory slots. There's no runtime allocation or dynamic slot assignment.

**Evidence:**
- `main/tropic_slot_map.h:36-57`:
  ```cpp
  // Compile-time TROPIC01 slot map (edit before build)
  // Syntax is always:
  //   ECC_SLOT_<MODULENAME>_START / ECC_SLOT_<MODULENAME>_END
  //   RMEM_SLOT_<MODULENAME>_START / RMEM_SLOT_<MODULENAME>_END
  
  #define ECC_SLOT_MOD_GPG_START 1
  #define ECC_SLOT_MOD_GPG_END 3
  #define ECC_SLOT_MOD_CA_START 4
  #define ECC_SLOT_MOD_CA_END 4
  #define ECC_SLOT_MOD_FIDO2_START 5
  #define ECC_SLOT_MOD_FIDO2_END 31
  
  #define TROPIC_ECC_SLOT_MAP(X) \
      X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
      X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
      X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)
  ```

- `components/cdc_core/src/TropicSlotMap.cpp`: Parses macros at compile time
- `main/CMakeLists.txt:8-19`: Module list is also compile-time

## Impact
**Deployment Inflexibility:** Adding a new module or changing slot allocation requires:
1. Editing `tropic_slot_map.h`
2. Ensuring module names match between CMakeLists.txt and slot map
3. Rebuilding entire firmware
4. Risk of slot collisions if modules are added/removed

Cannot support:
- Dynamic module loading
- Runtime slot reassignment
- Different slot configurations per deployment

## Evidence
Files affected:
- `main/tropic_slot_map.h:36-57` (slot definitions)
- `components/cdc_core/include/cdc_core/TropicSlotMap.h` (runtime validation)
- `main/CMakeLists.txt:8-19` (module configuration)

Module registration in `components/mod_totp/src/TotpModule.cpp:930-940`:
```cpp
bool TotpModule::init() {
    // ...
    core::ModuleRegistry::instance().registerModule(this);
    if (slotRange_.hasRmem) {
        TotpStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "TOTP slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    // ...
}
```
Module must wait for `setSlotRange()` from registry - no fallback allocation.

## Recommended Fix
Implement runtime slot allocation:

1. **Slot allocator service:**
   ```cpp
   class TropicSlotAllocator {
   public:
       struct Allocation {
           uint8_t eccStart, eccEnd;
           uint16_t rmemStart, rmemEnd;
       };
       Allocation allocate(const char* moduleName, uint8_t minEcc, uint16_t minRmem);
       void deallocate(const char* moduleName);
   };
   ```

2. **Default map as fallback:**
   ```cpp
   // Default map in header
   static constexpr SlotDef DEFAULT_MAP[] = {
       {"mod_gpg", 1, 3, 0, 0},
       {"mod_fido2", 5, 31, 132, 158},
       // ...
   };
   
   // Runtime allocator uses DEFAULT_MAP + NVS overrides
   ```

3. **NVS-based override:**
   ```cpp
   // Store custom allocation in NVS
   struct SlotOverride {
       char moduleName[16];
       uint8_t eccStart, eccEnd;
       uint16_t rmemStart, rmemEnd;
   };
   ```

This allows different deployments to have different slot configurations without rebuilding.

## References
- Memory pool allocation
- Resource management patterns
- Configuration-driven allocation
