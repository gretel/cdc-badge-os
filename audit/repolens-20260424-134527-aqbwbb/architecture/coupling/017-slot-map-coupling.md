---
title: "[LOW] TROPIC01 slot map creates compile-time coupling"
severity: LOW
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "memory-coupling"
  - "slot-allocation"
---

## Summary
The TROPIC01 secure element slot allocation is hardcoded in module registration, creating compile-time coupling between modules and memory layout. Changing slot assignments requires recompiling all modules.

**Evidence:**
- `components/mod_fido2/src/Fido2Module.cpp`: Requests specific ECC slot ranges
- `components/mod_totp/src/TotpModule.cpp`: Uses R-Memory slots 32-131
- `main/tropic_slot_map.h`: Compile-time slot allocation table
- Module registration in `main/CMakeLists.txt` determines slot order

## Impact
1. **Rigid slot allocation**: Cannot change slot assignments without recompiling
2. **Module ordering dependency**: Slots assigned based on registration order
3. **Hard to extend**: Adding new modules requires slot map changes
4. **Debugging difficulty**: Slot conflicts may only appear at runtime

## Evidence
File: `main/tropic_slot_map.h`
```cpp
// Compile-time slot allocation (example structure)
struct SlotMapEntry {
    const char* moduleName;
    uint8_t eccSlots;
    uint16_t rmemSlots;
};
```

File: `components/mod_fido2/src/Fido2Module.cpp`
```cpp
// Module requests specific slot range
SlotRequest getSlotRequest() const override {
    return {.minEccSlots = 10, .minRmemSlots = 20};
}
```

File: `main/CMakeLists.txt`
```cpp
// Module order determines slot assignment
set(MODULES
    grove_led
    mod_totp
    mod_fido2
    mod_password
    // ...
)
```

## Recommended Fix
1. **Use slot IDs instead of ranges**:
   ```cpp
   struct SlotRequest {
       const char* slotId;  // "fido2_key_1" instead of index
   };
   ```

2. **Runtime slot allocation**:
   ```cpp
   class SlotAllocator {
       uint8_t allocateEcc(uint16_t count, const char* moduleId);
       uint16_t allocateRmem(uint16_t count, const char* moduleId);
   };
   ```

3. **Configuration file**: Store slot map in NVS or config file, load at runtime

4. **Slot aliases**: Define logical slot names in module headers, resolve at runtime

## References
- Slot map: `main/tropic_slot_map.h`
- Module slot interface: `components/cdc_core/include/cdc_core/IModule.h:57-70`
- Module registry: `components/cdc_core/src/ModuleRegistry.cpp`
