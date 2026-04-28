---
title: "[MEDIUM] TR01_ECC_DEL allows deletion of any ECC slot without module ownership verification"
severity: MEDIUM
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The `TR01_ECC_DEL` command can delete any ECC key slot (0-31) without verifying module ownership or requiring specific authentication for each slot. A user with serial access can delete keys belonging to any module (GPG, FIDO2, etc.).

**Location:** `components/serial_cmd/src/SerialCmd.cpp:1476`

## Impact
- **GPG key destruction:** Can delete GPG keys (slots 1-3) without PW3 (Admin PIN) verification
- **FIDO2 credential loss:** Can delete FIDO2 credentials (slots 5-31) without re-registration
- **Attestation key loss:** Slot 0 holds the attestation key, essential for device identity
- **No cascade protection:** Deleting ECC slot doesn't clean up associated R-Memory metadata

## Evidence
Command registration at `components/serial_cmd/src/SerialCmd.cpp:1476`:
```cpp
reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmdTr01EccDel, "tr01", true});
```

While `requiresAuth = true`, this only checks the serial session PIN, not module-specific ownership.

Command handler at `components/serial_cmd/src/SerialCmd.cpp:1018-1032`:
```cpp
static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed\r\n");
    }
}
```

The handler accepts any slot number (0-31) and deletes it without:
1. Checking which module owns the slot
2. Verifying module-specific authentication (e.g., GPG PW3 for GPG slots)
3. Cleaning up associated R-Memory metadata

TROPIC01 slot allocation (from documentation):
```
| Module   | ECC Slots | R-Memory Slots |
|----------|-----------|----------------|
| SYSTEM   | 0         | 0 (PINs)       |
| GPG      | 1-3       | 1-3            |
| CA       | 4         | 4              |
| FIDO2    | 5-31      | 5-31           |
```

An attacker can:
1. Delete GPG keys (slots 1-3) with a single command
2. Delete FIDO2 credentials (slots 5-31)
3. Delete attestation key (slot 0)

## Recommended Fix
Add slot-specific ownership verification. Update `components/serial_cmd/src/SerialCmd.cpp`:

1. **Add slot ownership check:**
```cpp
static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    uint8_t slot = static_cast<uint8_t>(result.value);
    
    // Slot-specific access control
    auto& storage = core::TropicStorage::instance();
    core::TropicStorage::CacheEntry entry;
    
    if (!storage.getEntry(slot, &entry)) {
        Console::printf("ERROR: Slot %d is empty\r\n", slot);
        return;
    }
    
    // Get module name for this slot
    const char* moduleName = storage.getModuleName(entry.module_id);
    
    // Slot 0 (attestation) requires Admin PIN
    if (slot == 0) {
        char pw3[17] = {};
        Console::printf("Enter PW3 (Admin PIN) for slot 0: ");
        // ... read PW3 ...
        auto& pm = core::PinManager::instance();
        if (!pm.verifyPW3(pw3)) {
            Console::printf("ERROR: PW3 verification failed\r\n");
            return;
        }
    }
    // GPG slots (1-3) require GPG module authentication
    else if (strcmp(moduleName, "gpg") == 0) {
        // Verify GPG PW3
        // ...
    }
    // FIDO2 slots (5-31) require FIDO2 authentication
    else if (strcmp(moduleName, "fido2") == 0) {
        // Verify FIDO2 PIN
        // ...
    }
    
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed\r\n");
    }
}
```

2. **Add R-Memory cleanup:**
```cpp
// After ECC delete, also erase associated R-Memory slot
uint16_t rmemSlot = slot;  // ECC and R-Memory slots are paired
se->rmemErase(rmemSlot);
```

## References
- NIST SP 800-73-4 (PIV Interface): https://csrc.nist.gov/publications/detail/sp/800-73/4/final
- FIDO2 CTAP2 specification: https://fidoalliance.org/specs/fido2/
- OWASP Authorization Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Authorization_Cheat_Sheet.html
