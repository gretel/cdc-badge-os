---
title: "[MEDIUM] TROPIC01 slot map is hardcoded at compile-time with no runtime flexibility"
severity: MEDIUM
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
The TROPIC01 secure element slot allocation (ECC and R-Memory slots) is defined as compile-time macros in `main/tropic_slot_map.h` (lines 1-64). Adding a new module that requires secure element storage requires:

1. Editing the slot map header file
2. Adding new `#define` macros for slot ranges
3. Adding new entries to the `TROPIC_ECC_SLOT_MAP` and `TROPIC_RMEM_SLOT_MAP` macros
4. Rebuilding the entire firmware

This violates the Open/Closed Principle - the slot map system should be open for extension (adding new module allocations) without modifying the core system.

**Evidence:**
- `main/tropic_slot_map.h:54-62` - Hardcoded macro definitions for slot ranges
- `main/CMakeLists.txt:8-19` - Module list (only place modules are configured)
- `components/cdc_core/src/ModuleRegistry.cpp:870-912` - Slot validation logic that reads from the hardcoded map

## Impact
- **Maintenance Burden**: Every new module requiring secure element storage requires editing a core file
- **Error Prone**: Manual editing increases risk of slot conflicts or misconfiguration
- **Limited Flexibility**: Cannot dynamically allocate slots based on available resources or user preferences
- **Build Dependency**: Requires full rebuild even for simple slot reconfiguration

## Evidence
File: `main/tropic_slot_map.h:54-62`
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

File: `main/CMakeLists.txt:8-19`
```cmake
set(MODULES
    grove_led
    mod_totp
    mod_fido2
    mod_password
    mod_gpg
    mod_sao
    mod_vcard
    mod_ble_serial
    mod_nvsedit
    mod_hid
)
```

Note that `mod_sao`, `mod_vcard`, `mod_ble_serial`, `mod_nvsedit`, `mod_hid` are in the module list but have **no slot map entries** - they must request slots dynamically or use no slots at all.

## Recommended Fix
Create a runtime slot allocation system:

1. **Add a slot allocator interface** in `components/cdc_core/SlotAllocator.h`:
   ```cpp
   class SlotAllocator {
   public:
       virtual bool allocateEcc(const char* moduleName, uint16_t count, uint8_t& startSlot) = 0;
       virtual bool allocateRmem(const char* moduleName, uint16_t count, uint16_t& startSlot) = 0;
       virtual void releaseEcc(const char* moduleName) = 0;
       virtual void releaseRmem(const char* moduleName) = 0;
   };
   ```

2. **Add slot requirements to IModule interface** (in `IModule.h`):
   ```cpp
   struct SlotRequirements {
       uint8_t minEccSlots = 0;
       uint16_t minRmemSlots = 0;
   };
   virtual SlotRequirements getSlotRequirements() const;
   ```

3. **Implement dynamic allocation** in `ModuleRegistry` that:
   - Queries each module's slot requirements during init
   - Allocates slots from available pool
   - Reports conflicts as errors

4. **Store slot map in NVS** for persistence across reboots (optional, for flexibility)

This allows new modules to declare their slot needs without editing core files.

## References
- Open/Closed Principle: Classes should be open for extension, closed for modification
- Strategy Pattern: Encapsulate allocation algorithms for interchangeability
- ESP32-S3 has 32 ECC slots and 512 R-Memory slots - allocation should be flexible
