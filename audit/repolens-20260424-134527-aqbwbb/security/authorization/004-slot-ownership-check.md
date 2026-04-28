---
title: "[LOW] TROPIC01 slot commands lack per-slot ownership verification"
severity: LOW
domain: authorization
lens: slot-access
labels:
  - "slot-access"
  - "idor"
---

## Summary

The TROPIC01 secure element slot commands (`TR01_RMEM_READ`, `TR01_ECC_DEL`, `TR01_RMEM_DEL`) accept slot numbers as parameters but **do not verify module ownership** of the slot before executing. This is similar to an Insecure Direct Object Reference (IDOR) vulnerability.

Commands registered in `components/serial_cmd/src/SerialCmd.cpp`:
```cpp
// Lines 1474-1476
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmd_tr01_rmem_read, "tr01", false});
reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmd_tr01_ecc_del, "tr01", true});
reg.registerCommand({"TR01_RMEM_DEL", "Delete R-Memory slot", cmd_tr01_rmem_del, "tr01", true});
```

The commands check slot bounds but not ownership:
```cpp
// Line 990-1009: TR01_RMEM_READ
static void cmd_tr01_rmem_read(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    // ...
    hal::SeResult seResult = se->rmemRead(slot, data, sizeof(data), &actualLen);
    // ...
}

// Line 1011-1023: TR01_ECC_DEL
static void cmd_tr01_ecc_del(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    // ...
    hal::SeResult seResult = se->eccDelete(slot);
    // ...
}
```

## Impact

1. **Cross-module data access** - Any module can read R-Memory slots owned by other modules by specifying the slot number directly.

2. **Cross-module data deletion** - Any authenticated user can delete ECC or R-Memory slots owned by other modules.

3. **Module isolation bypass** - The slot map system (`TropicSlotMap`) is designed to assign specific slot ranges to specific modules, but this isolation is bypassed via serial commands.

## Evidence

**File: `components/serial_cmd/src/SerialCmd.cpp`**
- Lines 990-1011: `cmd_tr01_rmem_read` - reads any R-Memory slot
- Lines 1011-1023: `cmd_tr01_ecc_del` - deletes any ECC slot
- Lines 1025-1037: `cmd_tr01_rmem_del` - deletes any R-Memory slot

**File: `components/cdc_core/include/cdc_core/TropicSlotMap.h`**
- Slot map structure that defines module-specific slot ranges

**File: `components/cdc_core/src/TropicSlotMap.cpp`**
- Slot assignment logic that is bypassed by direct serial commands

## Recommended Fix

1. **Add slot ownership verification** - Check if the slot belongs to the requesting context:
   ```cpp
   static void cmd_tr01_rmem_read(const char* args) {
       auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
       if (!result.valid) {
           Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
           return;
       }
       
       uint16_t slot = static_cast<uint16_t>(result.value);
       
       // Check slot ownership (if slot map is available)
       auto& slotMap = core::TropicSlotMap::instance();
       const char* owner = slotMap.getOwner(slot);
       if (owner) {
           Console::printf("Slot %d owned by: %s\r\n", slot, owner);
       }
       
       // ... rest of read logic
   }
   ```

2. **Add a slot info command** to show ownership before accessing:
   ```cpp
   reg.registerCommand({"TR01_SLOT_INFO", "Show slot ownership", cmd_tr01_slot_info, "tr01", true});
   ```

3. **Consider restricting access** - Only allow reading/deleting slots that are:
   - Unassigned (empty)
   - Owned by the "system" module
   - Explicitly whitelisted for serial access

## References

- [OWASP Top 10 2021 - A01:2021 Broken Access Control](https://owasp.org/www-category-broken-access-control/)
- [OWASP Top 10 - Insecure Direct Object Reference](https://owasp.org/www-community/Insecure_Direct_Object_Reference_prevention)
