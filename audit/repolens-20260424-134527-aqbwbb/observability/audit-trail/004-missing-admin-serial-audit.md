---
title: "[HIGH] Missing audit trail for admin serial commands"
severity: HIGH
domain: administration
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary
Administrative serial commands that modify system state lack audit logging. `components/serial_cmd/src/SerialCmd.cpp` contains many admin commands that perform significant state changes without generating audit records.

**Missing audit events:**
1. NVS operations: `NVS_CLEAR`, `NVS_DEL`, `NVS_WRITE`
2. TROPIC01 operations: `TR01_ECC_DEL`, `TR01_RMEM_DEL`, `TR01_WIPE`, `TR01_CLEANUP`
3. Time operations: `SET_TIME`, `SET_DATE`
4. Module operations: `MODULE_ENABLE`, `MODULE_DISABLE`
5. Session authentication: `AUTH`, `LOGOUT`

## Impact
- **Privileged action tracking**: Admin commands can wipe all data, delete keys, or change system time without audit trail
- **Forensics**: After a destructive operation (e.g., `TR01_WIPE`), no record exists of when or who executed it
- **Compliance**: Administrative actions typically require audit trails for compliance
- **Accountability**: Cannot distinguish between user-initiated and admin-initiated changes

## Evidence
```cpp
// SerialCmd.cpp:485 - NVS clear (destructive)
static void cmdNvsClear(const char* args) {
    if (!args || strcmp(args, "YES") != 0) return;
    Console::printf("Clearing NVS...\r\n");
    esp_err_t err = nvs_flash_erase();  // Erases ALL NVS!
    // ...
    Console::printf("OK: NVS cleared. Reboot recommended.\r\n");
    // No audit event!
}

// SerialCmd.cpp:1130 - TROPIC01 wipe (destructive)
static void cmdTr01Wipe(const char* args) {
    // ...
    // Delete all ECC keys
    for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
        if (se->eccSlotUsed(i)) {
            if (se->eccDelete(i) == hal::SeResult::OK) {
                eccDeleted++;
            }
        }
    }
    // Erase R-Memory slots
    for (uint16_t i = 0; i < hal::ISecureElement::RMEM_SLOT_COUNT; i++) {
        if (se->rmemSlotUsed(i)) {
            if (se->rmemErase(i) == hal::SeResult::OK) {
                rmemDeleted++;
            }
        }
    }
    Console::printf("=== Factory Reset Complete ===\r\n");
    // No audit event for destructive wipe!
}

// SerialCmd.cpp:701 - Set time
static void cmdSetTime(const char* args) {
    // ... parse time ...
    if (setSystemTime(tm)) {
        Console::printf("OK: Time set to %02d:%02d:%02d\r\n", h, m, s);
        // No audit event for time change!
    }
}

// SerialCmd.cpp:823 - Authentication
static void cmdAuth(const char* args) {
    if (SerialCmd::authenticate(args)) {
        Console::printf("OK: Authenticated\r\n");
        // No audit event for auth success!
    } else {
        Console::printf("ERROR: Wrong PIN.\r\n");
        // No audit event for auth failure!
    }
}
```

## Recommended Fix
1. Create audit helper in `components/serial_cmd/include/serial_cmd/SerialCmdAudit.h`:
   ```cpp
   void serial_audit_command_executed(const char* cmd, bool success, const char* details);
   void serial_audit_auth_attempt(const char* pin_hint, bool success);
   void serial_audit_nvs_clear();
   void serial_audit_tr01_wipe(uint16_t ecc_count, uint16_t rmem_count);
   void serial_audit_time_change(const char* new_time);
   ```

2. Add calls to command handlers:
   - `cmdNvsClear()`: After successful NVS erase
   - `cmdTr01Wipe()`: After successful wipe
   - `cmdSetTime()` / `cmdSetDate()`: After time update
   - `cmdAuth()`: After authentication success/failure
   - `cmdLogout()`: After logout
   - `cmdNvsDel()`: After key deletion
   - `cmdTr01EccDel()` / `cmdTr01RmemDel()`: After slot deletion

3. Include in audit:
   - Command name
   - Success/failure
   - Timestamp
   - Relevant parameters (e.g., slot number, count of items deleted)

## References
- NIST SP 800-53 Rev 5: AU-2 (Audit Events)
- Common Criteria: FIA_UAU.4 (Single User Authentication)
- FIDO2 Server Implementation (Section 6.3 Audit Logging)
