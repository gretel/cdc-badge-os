---
title: "[LOW] Missing CRUD Operations for Module Data"
severity: LOW
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Several modules are missing standard CRUD (Create, Read, Update, Delete) operations, forcing users to use UI-only workflows for updates.

**Locations:**
- `components/mod_totp/src/TotpModule.cpp` - TOTP module (line 324-328)
- `components/mod_password/src/PasswordModule.cpp` - Password module (line 327-332)
- `components/mod_gpg/src/GpgModule.cpp` - GPG module (line 114-119)

## Impact
**Limited automation:** Serial command users cannot update existing entries without using the UI.

**Feature disparity:** UI has more capabilities than serial interface.

**Workarounds:** Users must DELETE and re-ADD to "update" entries.

## Evidence

### Current CRUD Coverage:

| Module | Create | Read | Update | Delete |
|--------|--------|------|--------|--------|
| TOTP | TOTP_ADD | TOTP_GET, TOTP_LIST | **MISSING** | TOTP_DEL |
| Password | PASSWORD_ADD | PASSWORD_GET, PASSWORD_LIST | **MISSING** | PASSWORD_DEL |
| GPG | GPG_GENERATE | GPG_STATUS | GPG_RESET (partial) | GPG_RESET |

### Missing Operations:

1. **TOTP Update:**
   ```cpp
   // UI supports editing (TotpModule.cpp:754-912)
   wizardEdit() - Full edit wizard
   
   // Serial has no update command
   // Current workaround: TOTP_DEL + TOTP_ADD
   ```

2. **Password Update:**
   ```cpp
   // UI supports editing (PasswordModule.cpp:576-665)
   wizardEdit() - Full edit wizard
   
   // Serial has no update command
   // Current workaround: PASSWORD_DEL + PASSWORD_ADD
   ```

3. **GPG Partial Updates:**
   ```cpp
   // Only full reset available
   GPG_RESET - Resets all keys
   
   // No way to update user_id without regenerating keys
   ```

### UI vs Serial Feature Gap:

**TOTP Module:**
- UI: Add, Edit, Delete, View
- Serial: Add, Get, List, Delete (no Edit)

**Password Module:**
- UI: Add, Edit, Delete, View
- Serial: Add, Get, List, Delete (no Edit)

## Recommended Fix

### Add UPDATE Commands

1. **TOTP_UPDATE:**
   ```cpp
   // Update specific fields
   TOTP_UPDATE <index> [name] [secret] [issuer] [digits] [period] [algo]
   
   // Or update all at once
   TOTP_UPDATE <index> <name> <secret> [issuer] [digits] [period] [algo]
   ```

2. **PASSWORD_UPDATE:**
   ```cpp
   PASSWORD_UPDATE <index> [title] [username] [password] [url] [totp] [notes]
   ```

3. **GPG_UPDATE (optional):**
   ```cpp
   // Update user_id without regenerating keys
   GPG_UPDATE_USER_ID <user_id>
   ```

### Implementation Example:

```cpp
// Add to TotpModule.cpp
static void cmd_totp_update(const char* args) {
    char indexBuf[8] = {};
    char nameBuf[TotpStore::NAME_LEN + 1] = {};
    char secretBuf[128] = {};
    char issuerBuf[TotpStore::ISSUER_LEN + 1] = {};
    char digitsBuf[8] = {};
    char periodBuf[8] = {};
    char algoBuf[8] = {};
    
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: TOTP_UPDATE <index> [name] [secret] [issuer] [digits] [period] [algo]\r\n");
        return;
    }
    
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: index not found\r\n");
        return;
    }
    
    // Read current values
    TotpAccount account = {};
    if (!TotpStore::instance().readAccount(slot, &account)) {
        cdc::serial::Console::printf("ERROR: read failed\r\n");
        return;
    }
    
    // Parse optional updates
    p = nextToken(p, nameBuf, sizeof(nameBuf));
    if (nameBuf[0]) strncpy(account.name, nameBuf, sizeof(account.name));
    
    p = nextToken(p, secretBuf, sizeof(secretBuf));
    if (secretBuf[0]) {
        // Parse and set new secret
    }
    
    // ... parse other fields
    
    bool ok = TotpStore::instance().updateAccount(slot, account);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

### Register New Commands:

```cpp
// In registerCommands() for each module
reg.registerCommand({"TOTP_UPDATE", "Update TOTP account", cmd_totp_update, CMD_MODULE, true});
reg.registerCommand({"PASSWORD_UPDATE", "Update password entry", cmd_password_update, CMD_MODULE, true});
```

### Alternative: Field-specific Updates

For more REST-like granularity:

```cpp
// Set individual fields
TOTP_SET_ISSUE <index> <issuer>
TOTP_SET_DIGITS <index> <digits>
PASSWORD_SET_URL <index> <url>
PASSWORD_SET_NOTES <index> <notes>
```

## References

- [REST API Design - Update Operations](https://restfulapi.net/http-methods/)
- [Google API Design - Update Methods](https://google.aip.dev/134)
