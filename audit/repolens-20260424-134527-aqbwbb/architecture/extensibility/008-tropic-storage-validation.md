---
title: "[LOW] TROPIC Storage Validation Uses Hardcoded Module Name Comparison"
severity: LOW
domain: extensibility
lens: storage-system
labels:
  - "audit:architecture/extensibility"
---

## Summary
The TROPIC storage validation in `components/cdc_core/src/TropicSlotMap.cpp` uses string comparison against hardcoded module names to validate slot allocation. This couples the storage system to specific module names.

**Evidence:**
- `components/cdc_core/src/TropicSlotMap.cpp` (validation logic):
  ```cpp
  bool TropicSlotMap::validateSlotMap(const char* moduleName) {
      // Compare against hardcoded map entries
      for (int i = 0; i < TROPIC_ECC_SLOT_MAP_COUNT; i++) {
          if (strcmp(moduleName, eccSlots_[i].moduleName) == 0) {
              // Found match
          }
      }
  }
  ```

- `components/mod_totp/src/TotpModule.cpp:930-940`:
  ```cpp
  bool TotpModule::init() {
      core::ModuleRegistry::instance().registerModule(this);
      if (slotRange_.hasRmem) {
          TotpStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
          core::ModuleRegistry::instance().clearModuleErrorByName(getName());
      } else {
          core::ModuleRegistry::instance().reportModuleError(getName(), "TOTP slot range missing");
          // ...
      }
  }
  ```

## Impact
**Fragile Coupling:**
1. Module name changes break slot validation
2. No way to alias module names
3. Hard to support module variants (e.g., `mod_totp_lite`)

## Evidence
Files affected:
- `components/cdc_core/src/TropicSlotMap.cpp` (validation)
- `components/mod_totp/src/TotpModule.cpp:930-940` (module init)
- `main/tropic_slot_map.h:36-57` (slot definitions)

## Recommended Fix
Use module ID instead of string comparison:

1. **Module ID-based lookup:**
   ```cpp
   bool TropicSlotMap::getRangeByModuleId(uint8_t moduleId, SlotType type, SlotRange* out) {
       for (int i = 0; i < count; i++) {
           if (eccSlots_[i].moduleId == moduleId) {
               out->valid = true;
               out->start = eccSlots_[i].start;
               out->end = eccSlots_[i].end;
               return true;
           }
       }
       return false;
   }
   ```

2. **Module provides ID:**
   ```cpp
   core::IModule::SlotRequest TotpModule::getSlotRequest() const {
       core::IModule::SlotRequest req = {};
       req.mapName = getName();
       req.minRmemSlots = 1;
       req.moduleId = MODULE_ID_MOD_TOTP;  // From module
       return req;
   }
   ```

## References
- Type-safe identifiers
- Hash-based lookups
