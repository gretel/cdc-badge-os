---
title: "[MEDIUM] Module ID assignments lack explicit documentation and gap analysis"
severity: MEDIUM
domain: database/schema-design
lens: identifier-allocation
labels:
  - "storage"
  - "module-ids"
  - "schema-design"
---

## Summary
Module IDs are defined in `tropic_slot_map.h` with some gaps (1, 7-254 are unused), but there's no documentation about the allocation strategy or which IDs are reserved for future use. This could lead to conflicts when adding new modules.

**Evidence:**
- `tropic_slot_map.h:28-35`:
  ```cpp
  #define MODULE_ID_MOD_SYSTEM 0
  #define MODULE_ID_MOD_GPG 2
  #define MODULE_ID_MOD_CA 3
  #define MODULE_ID_MOD_FIDO2 4
  #define MODULE_ID_MOD_TOTP 5
  #define MODULE_ID_MOD_PASSWORD 6
  #define MODULE_ID_UNKNOWN 255
  ```
  - ID 1 is unused
  - IDs 7-254 are unused
  - No documentation about allocation policy

## Impact
1. **Conflict risk**: New modules might accidentally use already-assigned IDs
2. **Fragmentation**: Gaps in ID space could lead to inefficient use
3. **Documentation debt**: Future developers won't know which IDs are available
4. **Reserved IDs**: No clear policy for system-reserved IDs (e.g., 0-10)

## Evidence
From `TropicStorage.cpp:168-178`:
```cpp
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    entry.moduleId = moduleId;
    // ...
}
```
No validation that moduleId is a recognized/allocated ID!

From `TropicSlotMap.cpp:64-70`:
```cpp
for (size_t i = 0; i < kSlotMapCount; i++) {
    const auto& a = kSlotMap[i];
    if (!a.moduleName || a.moduleName[0] == '\0') {
        setError("slot map entry with empty module name");
        return;
    }
    if (a.moduleId == MODULE_ID_UNKNOWN) {
        setError("slot map entry with unknown module id");
        return;
    }
    // ...
}
```
Only checks for UNKNOWN (255), not for duplicate or invalid IDs.

## Recommended Fix
1. **Document the allocation policy**:
   ```cpp
   // Module ID Allocation Policy:
   // 0: SYSTEM (reserved)
   // 1-10: Reserved for core modules
   // 11-50: Available for new modules
   // 51-100: Reserved for future core modules
   // 101-254: Available for user modules
   // 255: UNKNOWN (default)
   ```

2. **Add ID validation** in `TropicStorage::writeSlot()`:
   ```cpp
   bool isValidModuleId(uint8_t moduleId) {
       return moduleId != MODULE_ID_UNKNOWN && moduleId <= 254;
   }
   ```

3. **Create a module ID registry** header file:
   ```cpp
   // module_ids.h
   #define MODULE_ID_MOD_SYSTEM 0
   #define MODULE_ID_MOD_GPG 2
   // ...
   #define MODULE_ID_FIRST_AVAILABLE 7
   #define MODULE_ID_LAST_AVAILABLE 254
   ```

4. **Add compile-time check** for duplicate IDs:
   ```cpp
   static_assert(MODULE_ID_MOD_GPG != MODULE_ID_MOD_CA, "Duplicate module IDs");
   ```

## References
- UUID/GUID allocation patterns
- Embedded system identifier best practices
