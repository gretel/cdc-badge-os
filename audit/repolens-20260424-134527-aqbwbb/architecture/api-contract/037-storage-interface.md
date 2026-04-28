---
title: "[MEDIUM] Storage Interface Type Mismatches Between Modules"
severity: MEDIUM
domain: API Contract Integrity
lens: data-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
Different storage modules use inconsistent types and signatures for similar operations. FIDO2 uses `uint8_t` for slot indices while GPG uses `uint16_t`, and TOTP has its own slot management. This creates confusion when modules need to share or reference each other's storage.

**Location**:
- `components/mod_fido2/include/mod_fido2/fido2_storage.h:23-28` - FIDO2 slot types
- `components/mod_gpg/include/mod_gpg/GpgStorage.h:12-13` - GPG slot types
- `components/mod_totp/include/mod_totp/TotpStore.h:43-44` - TOTP slot types

## Impact
1. **Type confusion bugs**: Passing FIDO2 slot (uint8_t) to TOTP function expecting uint16_t
2. **Implicit truncation**: Large slot values may be silently truncated
3. **API discoverability**: Hard to find shared storage utilities due to inconsistent naming

## Evidence

**FIDO2 storage uses `uint8_t` for ECC slots**:
```cpp
// components/mod_fido2/include/mod_fido2/fido2_storage.h:23-28
void fido2_storage_set_slot_range(uint8_t ecc_start, uint8_t ecc_end,
                                  uint16_t rmem_start, uint16_t rmem_end);
uint8_t fido2_storage_ecc_start(void);
uint8_t fido2_storage_ecc_end(void);
uint16_t fido2_storage_rmem_start(void);
uint16_t fido2_storage_rmem_end(void);
```

**GPG storage uses `uint16_t` for all slots**:
```cpp
// components/mod_gpg/include/mod_gpg/GpgStorage.h:12-13
void gpg_storage_set_slot_range(uint16_t eccStart, uint16_t eccEnd);
void gpg_storage_set_rmem_range(uint16_t rmemStart, uint16_t rmemEnd);
```

**TOTP uses its own class with different signature**:
```cpp
// components/mod_totp/include/mod_totp/TotpStore.h:43-46
void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
```

**ISecureElement uses mixed types**:
```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h:75-148
virtual SeResult eccGenerate(uint8_t slot, EccCurve curve);  // uint8_t
virtual SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                          uint16_t* actualLen);  // uint16_t
```

No shared typedef or constant for slot types across modules.

## Recommended Fix

1. **Define shared slot types** in a common header:
   ```cpp
   // components/cdc_core/include/cdc_core/SlotTypes.h
   namespace cdc::core {
       using EccSlotIndex = uint8_t;   // 0-31
       using RmemSlotIndex = uint16_t; // 0-511
       
       constexpr EccSlotIndex ECC_SLOT_MAX = 31;
       constexpr RmemSlotIndex RMEM_SLOT_MAX = 511;
   }
   ```

2. **Update all storage interfaces** to use shared types:
   ```cpp
   // fido2_storage.h
   void fido2_storage_set_slot_range(cdc::core::EccSlotIndex eccStart,
                                     cdc::core::EccSlotIndex eccEnd,
                                     cdc::core::RmemSlotIndex rmemStart,
                                     cdc::core::RmemSlotIndex rmemEnd);
   ```

3. **Add type-safe wrappers** for common operations:
   ```cpp
   /**
    * \brief Get ECC slot count for a module.
    * \param start Start slot index.
    * \param end End slot index.
    * \return Number of slots (end - start + 1).
    */
   static inline uint8_t eccSlotCount(cdc::core::EccSlotIndex start,
                                      cdc::core::EccSlotIndex end) {
       return static_cast<uint8_t>(end - start + 1);
   }
   ```

4. **Document slot type assumptions** in each interface:
   ```cpp
   /**
    * \brief Set ECC slot range for FIDO2 credentials.
    * \param eccStart First ECC slot (0-31, uint8_t).
    * \param eccEnd Last ECC slot (0-31, uint8_t).
    * \param rmemStart First R-MEM slot (0-511, uint16_t).
    * \param rmemEnd Last R-MEM slot (0-511, uint16_t).
    * \note ECC slots use uint8_t (max 32 slots).
    * \note R-MEM slots use uint16_t (max 512 slots).
    */
   void fido2_storage_set_slot_range(uint8_t eccStart, uint8_t eccEnd,
                                     uint16_t rmemStart, uint16_t rmemEnd);
   ```

## References
- `components/mod_fido2/include/mod_fido2/fido2_storage.h:23-28` - FIDO2 slot types
- `components/mod_gpg/include/mod_gpg/GpgStorage.h:12-13` - GPG slot types
- `components/cdc_hal/include/cdc_hal/ISecureElement.h:75-148` - Secure element slot types
