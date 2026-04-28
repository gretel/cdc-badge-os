---
title: "[MEDIUM] TOTP Account Delete via Serial Command Lacks Confirmation"
severity: MEDIUM
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The TOTP module's serial command `TOTP_DEL` executes immediate deletion of a TOTP account without any confirmation step. The delete handler `cmd_totp_del()` in `components/mod_totp/src/TotpModule.cpp:268-282` directly calls `TotpStore::deleteAccount()` with no intermediate confirmation dialog or prompt.

**Evidence:**
- File: `components/mod_totp/src/TotpModule.cpp`
- Lines: 268-282
- Function: `cmd_totp_del(const char* args)`

```cpp
static void cmd_totp_del(const char* args) {
    if (!args || !*args) {
        cdc::serial::Console::printf("Usage: TOTP_DEL <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(args));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: index not found\r\n");
        return;
    }
    bool ok = TotpStore::instance().deleteAccount(slot);  // Immediate delete!
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The UI menu in `TotpModule.cpp` does not expose a delete function, but the serial command provides direct access to the destructive operation.

## Impact
- **Data Loss Risk**: Users can accidentally delete TOTP accounts via serial console with a single command
- **No Recovery**: TOTP secrets are stored in secure element R-Memory; once erased, accounts cannot be restored without re-adding from backup
- **Inconsistent UX**: Other modules (like Password) implement confirmation dialogs for delete operations, but TOTP does not

## Recommended Fix
Add a confirmation step to the `cmd_totp_del()` serial command handler. Since serial commands are text-based, implement a two-step confirmation pattern similar to `TR01_WIPE`:

1. First invocation shows a warning with the account name and requires `CONFIRM` argument
2. Second invocation with `CONFIRM` executes the actual delete

Example implementation:
```cpp
static void cmd_totp_del(const char* args) {
    if (!args || !*args) {
        cdc::serial::Console::printf("Usage: TOTP_DEL <index> [CONFIRM]\r\n");
        return;
    }
    
    // Parse index
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    
    // Get account name for display
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: index not found\r\n");
        return;
    }
    
    TotpAccount account = {};
    TotpStore::instance().readAccount(slot, &account);
    
    // Check if confirmed
    if (p && strcmp(p, "CONFIRM") == 0) {
        bool ok = TotpStore::instance().deleteAccount(slot);
        cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
        return;
    }
    
    // Show confirmation prompt
    cdc::serial::Console::printf("WARNING: Delete TOTP account '%s'?\r\n", account.name);
    cdc::serial::Console::printf("  This will permanently remove the secret.\r\n");
    cdc::serial::Console::printf("  To proceed, type: TOTP_DEL %d CONFIRM\r\n", index);
}
```

## References
- Related issue: Password module implements confirmation via `showConfirm()` in `mod_password/src/PasswordModule.cpp:699`
- Similar pattern: `TR01_WIPE` command in `serial_cmd/src/SerialCmd.cpp:1132` uses `CONFIRM` argument pattern
