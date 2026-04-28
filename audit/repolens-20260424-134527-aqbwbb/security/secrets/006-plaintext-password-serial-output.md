---
title: "[MEDIUM] Password vault entries printed in plaintext via serial console"
severity: MEDIUM
domain: secrets
lens: secrets-credential-management
labels:
  - "audit:security/secrets"
---

## Summary
The `PASSWORD_GET` serial command outputs password entries in plaintext to the serial console, including the password field. This is in `components/mod_password/src/PasswordModule.cpp:238`.

## Impact
**Security Risk**: When a user queries a password entry via serial command, the full password is printed. This is useful for debugging but creates risks:
- Serial logs may be captured in terminal history
- Serial output may be logged by IDE/monitor tools
- Anyone with serial access can extract all stored passwords
- No masking or truncation of sensitive fields

## Evidence

**File: `components/mod_password/src/PasswordModule.cpp`**
```cpp
// Lines 220-245 - PASSWORD_GET command
static void cmd_password_get(const char* args) {
    char indexBuf[4] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !*indexBuf) {
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
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);  // <-- PLAINTEXT PASSWORD
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    if (entry.totpSlot == PasswordStore::TOTP_SLOT_NONE) {
        cdc::serial::Console::printf("TOTP Slot: none\r\n");
    } else {
        cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
    }
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}
```

## Recommended Fix
1. **Add a mask option**: Add a flag to PASSWORD_GET to show masked passwords:
   ```
   PASSWORD_GET <index>           // Shows masked password (e.g., "**********")
   PASSWORD_GET <index> --verbose // Shows full password (opt-in)
   ```

2. **Add a dedicated copy command**: For getting plaintext, use a separate command that requires additional auth:
   ```
   PASSWORD_COPY <index>          // Outputs just the password for clipboard
   ```

3. **Add configurable masking**: Make masking configurable via settings.

4. **Add warning**: At minimum, add a warning comment in the code about serial output being plaintext.

## References
- [OWASP Secrets Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Secrets_Management_Cheat_Sheet.html) - Secrets should be masked in logs
- [NIST SP 800-53 IA-5](https://nvd.nist.gov/800-53/Rev4/control/IA-5) - Authentication factor protection
- [ CWE-532: Information Exposure Through Log Files](https://cwe.mitre.org/data/definitions/532.html)
