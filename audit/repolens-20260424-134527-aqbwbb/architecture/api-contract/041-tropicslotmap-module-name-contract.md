---
title: "[MEDIUM] TropicSlotMap Module Name Contract Missing Runtime Validation"
severity: MEDIUM
domain: API Contract Integrity
lens: slot-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
`TropicSlotMap` validates module names at compile-time using `tropic_slot_map.h`, but modules register at runtime with `getName()` which may differ from the compile-time names. This creates a potential mismatch between slot allocation and module lookup.

**Location**: 
- `main/tropic_slot_map.h` - Compile-time slot definitions
- `components/cdc_core/include/cdc_core/TropicSlotMap.h:28-29` - Runtime lookup API
- Module `getName()` implementations (e.g., `components/mod_gpg/src/GpgModule.h:9`)

## Impact
1. **Silent slot allocation failures**: Module name mismatch causes lookup to fail
2. **Debugging difficulty**: Hard to trace why a module can't find its slots
3. **Configuration drift**: Developers may change module names without updating slot map

## Evidence

In `main/tropic_slot_map.h:55-57`:
```cpp
#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)
```

In `main/tropic_slot_map.h:59-61`:
```cpp
#define TROPIC_RMEM_SLOT_MAP(X) \
    X("mod_totp", MODULE_ID_MOD_TOTP, RMEM_SLOT_MOD_TOTP_START, RMEM_SLOT_MOD_TOTP_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, RMEM_SLOT_MOD_FIDO2_START, RMEM_SLOT_MOD_FIDO2_END) \
    X("mod_password", MODULE_ID_MOD_PASSWORD, RMEM_SLOT_MOD_PASSWORD_START, RMEM_SLOT_MOD_PASSWORD_END)
```

**Note**: `mod_gpg` has ECC slots but NO RMEM slots defined!

In `components/mod_gpg/include/mod_gpg/GpgModule.h:9`:
```cpp
const char* getName() const override { return "mod_gpg"; }
```

In `components/mod_gpg/src/GpgModule.cpp:550-560`:
```cpp
bool GpgModule::init() {
    // ...
    core::ModuleRegistry::instance().registerModule(this);
    if (!slotRange_.hasEcc) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "GPG slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    gpg_storage_set_slot_range(slotRange_.eccStart, slotRange_.eccEnd);
    if (slotRange_.hasRmem) {  // <-- hasRmem will be false!
        gpg_storage_set_rmem_range(slotRange_.rmemStart, slotRange_.rmemEnd);
    }
    // ...
}
```

**The contract assumes**:
1. Module name in `tropic_slot_map.h` matches `getName()`
2. No typos or case differences
3. All modules that need R-MEM have it defined

**But there's no validation** that:
1. Every registered module has a slot map entry
2. Every slot map entry has a corresponding module
3. Module names are consistent

In `components/cdc_core/src/TropicSlotMap.cpp:138-152`:
```cpp
bool TropicSlotMap::getRangeByName(const char* moduleName, SlotType type, SlotRange* out) const {
    if (!moduleName || !out) return false;
    for (size_t i = 0; i < kSlotMapCount; i++) {
        const auto& entry = kSlotMap[i];
        if (entry.type != type) continue;
        if (strcmp(entry.moduleName, moduleName) != 0) continue;  // <-- String comparison
        // ...
    }
    return false;  // <-- Silent failure if not found
}
```

If a module has a typo in its name (e.g., "mod_gpp" instead of "mod_gpg"), it silently fails.

## Recommended Fix

1. **Add runtime validation** of module registration:
   ```cpp
   /**
    * \brief Validate that all registered modules have slot map entries.
    * \return true if all modules have valid slot assignments.
    */
   bool TropicSlotMap::validateModuleCoverage() const {
       // Check each registered module has a slot map entry
       for (uint8_t i = 0; i < ModuleRegistry::MAX_MODULES; i++) {
           auto* module = ModuleRegistry::instance().getModuleAt(i);
           if (!module) continue;
           
           const char* name = module->getName();
           auto req = module->getSlotRequest();
           
           if (req.minEccSlots > 0) {
               SlotRange range;
               if (!getRangeByName(name, SlotType::ECC, &range)) {
                   LOG_E(TAG, "Module %s has ECC requirement but no slot map", name);
                   return false;
               }
           }
           
           if (req.minRmemSlots > 0) {
               SlotRange range;
               if (!getRangeByName(name, SlotType::RMEM, &range)) {
                   LOG_E(TAG, "Module %s has RMEM requirement but no slot map", name);
                   return false;
               }
           }
       }
       return true;
   }
   ```

2. **Add warning for unused slot map entries**:
   ```cpp
   /**
    * \brief Check for slot map entries without corresponding modules.
    */
   void TropicSlotMap::checkOrphanedEntries() const {
       for (size_t i = 0; i < kSlotMapCount; i++) {
           const auto& entry = kSlotMap[i];
           auto* module = ModuleRegistry::instance().getModule(entry.moduleName);
           if (!module) {
               LOG_W(TAG, "Slot map entry %s has no registered module", entry.moduleName);
           }
       }
   }
   ```

3. **Document the naming contract**:
   ```cpp
   /**
    * \brief TROPIC01 slot map configuration.
    *
    * Contract:
    * - Module names in tropic_slot_map.h MUST match IModule::getName()
    * - Names are case-sensitive (e.g., "mod_gpg" not "mod_GPG")
    * - Every module with slot requirements MUST have an entry
    * - Every entry MUST have a corresponding registered module
    *
    * Validation:
    * - Run TropicSlotMap::validateModuleCoverage() at boot
    * - Check logs for "slot map" warnings
    */
   ```

4. **Add build-time check** (optional):
   ```cpp
   // Add to CMakeLists.txt or generate a header
   // Compare tropic_slot_map.h names with module getName() implementations
   ```

## References
- `main/tropic_slot_map.h:55-61` - Compile-time slot definitions
- `components/cdc_core/src/TropicSlotMap.cpp:138-152` - Runtime lookup
- `components/mod_gpg/include/mod_gpg/GpgModule.h:9` - Module name
