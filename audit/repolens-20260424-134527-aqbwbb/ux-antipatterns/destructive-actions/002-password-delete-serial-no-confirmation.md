---
title: "[MEDIUM] Password Entry Delete via Serial Command Lacks Confirmation"
severity: MEDIUM
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The Password module's serial command `PASSWORD_DEL` executes immediate deletion of a password entry without any confirmation step. While the UI-based delete flow in `PasswordModule.cpp` correctly uses a confirmation dialog (`showConfirm()` at line 699), the serial command `cmd_password_del()` bypasses this protection entirely.

**Evidence:**
- File: `components/mod_password/src/PasswordModule.cpp`
- Lines: 307-321
- Function: `cmd_password_del(const char* args)`

```cpp
static void cmd_password_del(const char* args) {
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_DEL <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    bool ok = PasswordStore::instance().deleteEntry(slot);  // Immediate delete!
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Compare to the UI-based delete at line 699 which properly uses confirmation:
```cpp
static void onMenuDelete() {
    static uint16_t slot = 0;
    slot = s_activeSlot;
    ui::showConfirm(mstr(STR_CONFIRM_DELETE), onMenuDeleteConfirm, nullptr,
                    ui::ConfirmView::Icon::WARNING, &slot);
}
```

## Impact
- **Data Loss Risk**: Password entries can be accidentally deleted via serial console with a single command
- **Inconsistent UX**: The UI flow properly warns users with a confirmation dialog, but the serial command provides a "fast track" that bypasses this protection
- **High-Value Target**: Password entries contain sensitive credentials; accidental deletion requires re-adding from external sources

## Recommended Fix
Add a confirmation step to the `cmd_password_del()` serial command handler using a two-step pattern:

```cpp
static void cmd_password_del(const char* args) {
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_DEL <index> [CONFIRM]\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    
    // Get entry details for display
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    
    PasswordEntry entry = {};
    PasswordStore::instance().readEntry(slot, &entry);
    
    // Check if confirmed
    p = nextToken(p, indexBuf, sizeof(indexBuf));  // Reuse buffer for "CONFIRM"
    if (p && strcmp(indexBuf, "CONFIRM") == 0) {
        bool ok = PasswordStore::instance().deleteEntry(slot);
        cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
        return;
    }
    
    // Show confirmation prompt
    cdc::serial::Console::printf("WARNING: Delete password entry '%s'?\r\n", entry.title);
    cdc::serial::Console::printf("  This will permanently remove username, password, and notes.\r\n");
    cdc::serial::Console::printf("  To proceed, type: PASSWORD_DEL %d CONFIRM\r\n", index);
}
```

## References
- UI confirmation pattern: `components/mod_password/src/PasswordModule.cpp:699`
- Similar serial command pattern: `TR01_WIPE` in `serial_cmd/src/SerialCmd.cpp:1132`
