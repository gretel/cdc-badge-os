---
title: "[HIGH] Password module serial command outputs full credentials (username, password, URL) to console"
severity: HIGH
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The password module's `PASSWORD_GET` serial command outputs complete password entry data including username, password, URL, and notes to the serial console. This exposes sensitive credentials in plaintext to anyone with serial access.

**Location:** `components/mod_password/src/PasswordModule.cpp` (lines 236-245)

```cpp
cdc::serial::Console::printf("Title: %s\r\n", entry.title);
cdc::serial::Console::printf("Username: %s\r\n", entry.username);
cdc::serial::Console::printf("Password: %s\r\n", entry.password);
cdc::serial::Console::printf("URL: %s\r\n", entry.url);
...
cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
```

## Impact
- **Complete Credential Exposure:** The actual password (not just metadata) is printed to the serial console, accessible to anyone connected via USB serial.
- **Multiple PII Fields:** Username, URL, and notes are also exposed, which may contain additional personal or organizational information.
- **No Protection Layer:** Unlike the display UI which may require PIN verification, serial commands may be accessible without strong authentication depending on configuration.
- **Log Capture Risk:** Serial output is commonly captured during debugging, troubleshooting, or automated testing, creating persistent records of credentials.
- **Password Manager Purpose Defeated:** A password manager that outputs passwords to a terminal defeats the purpose of secure credential storage.

## Evidence
**File:** `components/mod_password/src/PasswordModule.cpp`
**Lines 218-246:**

```cpp
static void cmd_password_get(const char* args) {
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_GET <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        cdc::serial::Console::printf("ERROR: read failed\r\n");
        return;
    }
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    if (entry.totpSlot == PasswordStore::TOTP_SLOT_NONE) {
        cdc::serial::Console::printf("TOTP Slot: none\r\n");
    } else {
        cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
    }
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}
```

The `PasswordEntry` struct (defined in `PasswordStore.h`) contains:
- `title`: Service/account name
- `username`: Login username (PII)
- `password`: **Actual password** (highly sensitive)
- `url`: Website URL
- `notes`: Additional notes (may contain PII)

## Recommended Fix
1. **Remove password output from serial command:**
   ```cpp
   cdc::serial::Console::printf("Title: %s\r\n", entry.title);
   cdc::serial::Console::printf("Username: %s\r\n", entry.username);
   cdc::serial::Console::printf("URL: %s\r\n", entry.url);
   cdc::serial::Console::printf("Password: [use display to view]\r\n");
   ```

2. **Add optional flag for password display:**
   ```cpp
   // PASSWORD_GET <index> [--show-password]
   bool showPassword = strstr(args, "--show-password") != nullptr;
   if (showPassword) {
       cdc::serial::Console::printf("Password: %s\r\n", entry.password);
   } else {
       cdc::serial::Console::printf("Password: [use --show-password to display]\r\n");
   }
   ```

3. **Require explicit confirmation:** Add a step that requires user confirmation before showing the password.

4. **Consider using display for sensitive data:** Show passwords only on the E-Paper display where they're less likely to be captured.

5. **Add DEBUG_MODE guard:**
   ```cpp
   #ifdef DEBUG_MODE
   cdc::serial::Console::printf("Password: %s\r\n", entry.password);
   #endif
   ```

6. **Document the command behavior:** Clearly document in the command help that passwords are hidden by default.

## References
- GDPR Article 5(1)(c) - Data minimization: "Personal data shall be adequate, relevant and limited to what is necessary"
- NIST SP 800-63B - Digital Identity Guidelines: Credential storage and display best practices
- OWASP Password Storage Cheat Sheet
- Common password manager UX patterns: Hide passwords by default, show on demand

</content>