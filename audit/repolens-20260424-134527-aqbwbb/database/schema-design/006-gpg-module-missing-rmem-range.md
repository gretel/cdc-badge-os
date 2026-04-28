---
title: "[HIGH] GPG module uses R-Memory but not registered in slot map"
severity: HIGH
domain: database/schema-design
lens: slot-allocation
labels:
  - "storage"
  - "gpg"
  - "slot-map"
---

## Summary
The GPG module uses R-Memory slot 0 (relative to its configured range) for storing the encrypted DEC private key (`GpgStorage.cpp:45`), but GPG is NOT listed in the RMEM slot map in `tropic_slot_map.h`. This means the slot validation logic in `TropicSlotMap::isRmemAllowedForModuleId()` will not recognize GPG's RMEM range.

**Evidence:**
- `tropic_slot_map.h:54-56`:
  ```cpp
  #define TROPIC_ECC_SLOT_MAP(X) \
      X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
      X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
      X("mod_fido2", MODULE_ID_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)
  ```
  GPG is in ECC map but NOT in RMEM map!

- `tropic_slot_map.h:58-60`:
  ```cpp
  #define TROPIC_RMEM_SLOT_MAP(X) \
      X("mod_totp", MODULE_ID_MOD_TOTP, RMEM_SLOT_MOD_TOTP_START, RMEM_SLOT_MOD_TOTP_END) \
      X("mod_fido2", MODULE_ID_MOD_FIDO2, RMEM_SLOT_MOD_FIDO2_START, RMEM_SLOT_MOD_FIDO2_END) \
      X("mod_password", MODULE_ID_MOD_PASSWORD, RMEM_SLOT_MOD_PASSWORD_START, RMEM_SLOT_MOD_PASSWORD_END)
  ```
  No GPG entry!

- `GpgStorage.cpp:42-46`:
  ```cpp
  static constexpr uint16_t RMEM_SLOT_DEC_KEY = 0;
  // ...
  uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;
  ```

- `GpgStorage.cpp:194-198`:
  ```cpp
  void gpg_storage_set_rmem_range(uint16_t rmemStart, uint16_t rmemEnd) {
      s_storage.rmemStart = rmemStart;
      s_storage.rmemEnd = rmemEnd;
  }
  ```
  GPG expects RMEM range to be configured at runtime!

## Impact
1. **Slot validation fails**: `TropicStorage::isEntryAllowed()` will return `false` for GPG RMEM slots
2. **Cache rebuild issues**: `TropicStorage::rebuild()` may skip GPG's DEC key slot
3. **Cleanup issues**: `TropicStorage::cleanup()` may incorrectly remove GPG's DEC key
4. **Runtime dependency**: GPG slot range must be manually configured at runtime, not validated at compile time

## Evidence
From `TropicStorage.cpp:470-474`:
```cpp
bool TropicStorage::isEntryAllowed(uint16_t slot, uint8_t moduleId) const {
    return TropicSlotMap::instance().isRmemAllowedForModuleId(slot, moduleId);
}
```

From `TropicSlotMap.cpp:183-192`:
```cpp
bool TropicSlotMap::isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const {
    SlotRange range;
    if (!getRangeByModuleId(moduleId, SlotType::RMEM, &range)) {
        return false;  // GPG will always return false!
    }
    return slot >= range.start && slot <= range.end;
}
```

## Recommended Fix
1. **Add GPG RMEM slot range** to `tropic_slot_map.h`:
   ```cpp
   #define RMEM_SLOT_MOD_GPG_START 33  // Or appropriate slot
   #define RMEM_SLOT_MOD_GPG_END 33    // Single slot for DEC key
   ```

2. **Add to RMEM map**:
   ```cpp
   #define TROPIC_RMEM_SLOT_MAP(X) \
       X("mod_gpg", MODULE_ID_MOD_GPG, RMEM_SLOT_MOD_GPG_START, RMEM_SLOT_MOD_GPG_END) \
       X("mod_totp", MODULE_ID_MOD_TOTP, RMEM_SLOT_MOD_TOTP_START, RMEM_SLOT_MOD_TOTP_END) \
       X("mod_fido2", MODULE_ID_MOD_FIDO2, RMEM_SLOT_MOD_FIDO2_START, RMEM_SLOT_MOD_FIDO2_END) \
       X("mod_password", MODULE_ID_MOD_PASSWORD, RMEM_SLOT_MOD_PASSWORD_START, RMEM_SLOT_MOD_PASSWORD_END)
   ```

3. **Update GPG storage** to use absolute slot instead of relative if only one slot needed

## References
- TROPIC01 slot allocation best practices
- Module isolation patterns
