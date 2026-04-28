---
title: "[MEDIUM] TOTP and Password module serial commands expose sensitive data without role-based check"
severity: MEDIUM
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The TOTP and Password modules' serial commands (`TOTP_ADD`, `PASSWORD_GET`, `PASSWORD_ADD`, `PASSWORD_DEL`) are registered with `requiresAuth = true`, but this only verifies the serial session PIN. There is no additional verification for accessing sensitive data like TOTP secrets and password entries.

**Location:** 
- TOTP: `components/mod_totp/src/TotpModule.cpp:313-319`
- Password: `components/mod_password/src/PasswordModule.cpp:323-329`

## Impact
- **TOTP secret exposure:** `TOTP_LIST` and `TOTP_GET` can enumerate and retrieve TOTP account data
- **Password retrieval:** `PASSWORD_GET` returns full password entries including plaintext passwords
- **Bulk data access:** Serial commands can iterate through all entries without additional verification
- **Missing data-level authorization:** Module-level PIN (badge PIN) doesn't protect module-specific sensitive data

## Evidence
TOTP command registration at `components/mod_totp/src/TotpModule.cpp:313-319`:
```cpp
static void registerCommands() {
    if (s_commandsRegistered) return;
    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
    reg.registerCommand({"TOTP_ADD", "Add TOTP account", cmd_totp_add, CMD_MODULE, true});
    reg.registerCommand({"TOTP_DEL", "Delete TOTP account by index", cmd_totp_del, CMD_MODULE, true});
    reg.registerCommand({"TOTP_GET", "Generate TOTP code by index", cmd_totp_get, CMD_MODULE, true});
    s_commandsRegistered = true;
}
```

Password command registration at `components/mod_password/src/PasswordModule.cpp:323-329`:
```cpp
static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;
    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
    reg.registerCommand({"PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true});
    reg.registerCommand({"PASSWORD_ADD", "Add password entry", cmd_password_add, CMD_MODULE, true});
    reg.registerCommand({"PASSWORD_DEL", "Delete password entry", cmd_password_del, CMD_MODULE, true});
}
```

`PASSWORD_GET` returns plaintext password at `components/mod_password/src/PasswordModule.cpp:225-235`:
```cpp
static void cmd_password_get(const char* args) {
    // ... argument parsing ...
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        cdc::serial::Console::printf("ERROR: read failed\r\n");
        return;
    }
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);  // Plaintext!
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    // ...
}
```

`TOTP_LIST` enumerates all accounts at `components/mod_totp/src/TotpModule.cpp:193-216`:
```cpp
static void cmd_totp_list(const char* args) {
    // ...
    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
        auto* c = static_cast<ListCtx*>(user);
        // ...
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", c->idx, entry.name, logical);
        c->idx++;
    };
    // Iterates through ALL TOTP accounts
}
```

## Recommended Fix
Add module-specific authorization for sensitive operations:

1. **For TOTP module** - Verify badge PIN is set and add optional TOTP-specific PIN:
```cpp
static void cmd_totp_list(const char* args) {
    // Verify session authenticated
    auto& pm = core::PinManager::instance();
    if (!pm.isPinSet()) {
        cdc::serial::Console::printf("ERROR: Badge PIN not configured\r\n");
        return;
    }
    // ... rest of command ...
}
```

2. **For Password module** - Require additional verification for password retrieval:
```cpp
static void cmd_password_get(const char* args) {
    // Verify session authenticated
    auto& pm = core::PinManager::instance();
    if (!pm.isPinSet()) {
        cdc::serial::Console::printf("ERROR: Badge PIN not configured\r\n");
        return;
    }
    
    // Optional: Add password vault PIN verification
    // char vaultPin[17] = {};
    // Console::printf("Enter vault PIN: ");
    // ... read and verify ...
    
    // ... rest of command ...
}
```

3. **Consider adding module-specific PINs:**
- Add a "vault PIN" for password module access
- Add a "TOTP PIN" for TOTP account management
- Store these in separate R-Memory slots with their own verification

## References
- OWASP Data Protection Cheat Sheet: https://cheatsheetseries.owasp.org/cheatsheets/Data_Protection_Cheat_Sheet.html
- NIST SP 800-63B Digital Identity Guidelines: https://pages.nist.gov/800-63-3/sp800-63b.html
- Multi-factor authentication best practices: https://www.nist.gov/news-events/news/2016/04/nist-digital-identity-guidelines-encourage-use-multifactor-authentication
