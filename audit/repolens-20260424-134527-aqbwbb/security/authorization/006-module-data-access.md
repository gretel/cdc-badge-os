---
title: "[LOW] TOTP and Password serial commands lack per-entry authorization"
severity: LOW
domain: authorization
lens: module-data
labels:
  - "totp"
  - "password"
  - "data-access"
---

## Summary

The TOTP and Password module serial commands (`TOTP_GET`, `TOTP_DEL`, `PASSWORD_GET`, `PASSWORD_DEL`) accept **user-supplied index parameters** to access specific entries but **do not verify ownership** or provide per-entry access control. All entries are accessible to any authenticated user.

From `components/mod_totp/src/TotpModule.cpp`:
```cpp
// Lines 280-290: TOTP_DEL
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
    bool ok = TotpStore::instance().deleteAccount(slot);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

From `components/mod_password/src/PasswordModule.cpp`:
```cpp
// Lines 304-317: PASSWORD_DEL
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
    bool ok = PasswordStore::instance().deleteEntry(slot);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The `findSlotByIndex()` function resolves the index to a slot but provides **no ownership verification**.

## Impact

1. **No per-entry isolation** - In a multi-user scenario (if the badge is shared), any authenticated user can access, edit, or delete any TOTP account or password entry.

2. **Predictable indices** - Entries are indexed sequentially (0, 1, 2...), making enumeration trivial.

3. **No audit trail** - There's no logging of which entry was accessed or modified.

## Evidence

**File: `components/mod_totp/src/TotpModule.cpp`**
- Lines 159-184: `findSlotByIndex()` - resolves index to slot without ownership check
- Lines 280-290: `cmd_totp_del` - deletes by index
- Lines 288-298: `cmd_totp_get` - gets code by index

**File: `components/mod_password/src/PasswordModule.cpp`**
- Lines 165-183: `findSlotByIndex()` - resolves index to slot without ownership check
- Lines 304-317: `cmd_password_del` - deletes by index
- Lines 219-245: `cmd_password_get` - gets entry by index

**File: `components/mod_totp/src/TotpStore.cpp`**
- Storage implementation with no per-entry access control

**File: `components/mod_password/src/PasswordStore.cpp`**
- Storage implementation with no per-entry access control

## Recommended Fix

1. **Add entry-level metadata** - Store a "owner" or "category" field for each entry:
   ```cpp
   struct TotpAccount {
       char name[TotpStore::NAME_LEN + 1];
       char issuer[TotpStore::ISSUER_LEN + 1];
       uint8_t secret[TotpStore::SECRET_MAX_LEN];
       uint8_t secretLen;
       uint8_t digits;
       uint32_t period;
       uint8_t algorithm;
       uint8_t category;  // New: user-defined category for grouping
   };
   ```

2. **Add filtering commands** - Allow filtering by category:
   ```cpp
   reg.registerCommand({"TOTP_LIST", "List TOTP accounts [category]", cmd_totp_list, CMD_MODULE, true});
   // cmd_totp_list checks optional category filter
   ```

3. **Add entry metadata command** - Show entry details including owner/category:
   ```cpp
   reg.registerCommand({"TOTP_INFO", "Show TOTP account info", cmd_totp_info, CMD_MODULE, true});
   ```

4. **Consider entry-level locking** - Add a "locked" flag that prevents deletion/modification:
   ```cpp
   struct PasswordEntry {
       // ... existing fields
       bool locked;  // Prevents delete/edit
   };
   
   reg.registerCommand({"PASSWORD_LOCK", "Lock password entry", cmd_password_lock, CMD_MODULE, true});
   reg.registerCommand({"PASSWORD_UNLOCK", "Unlock password entry", cmd_password_unlock, CMD_MODULE, true});
   ```

## References

- [OWASP Top 10 - Broken Access Control](https://owasp.org/www-category-broken-access-control/)
- [OWASP - Insecure Direct Object Reference](https://owasp.org/www-community/Insecure_Direct_Object_Reference_prevention)
