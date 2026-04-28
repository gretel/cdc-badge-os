---
title: "[HIGH] Password entry fully exposed via serial command PASSWORD_GET"
severity: HIGH
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `PASSWORD_GET` serial command in `components/mod_password/src/PasswordModule.cpp:236-245` outputs the complete password entry including the plaintext password field directly to the serial console without any masking or redaction.

## Impact
- **Credential Leakage**: The full password is printed to the serial output, visible to anyone with serial access (USB-CDC connection at 115200 baud)
- **Debug Artifact Exposure**: Serial output is commonly logged, captured, or forwarded to terminals, potentially persisting the password in logs
- **No Access Control**: The command is registered without authentication requirements, allowing any connected serial client to retrieve passwords

## Evidence
File: `components/mod_password/src/PasswordModule.cpp:236-245`
```cpp
cdc::serial::Console::printf("Title: %s\r\n", entry.title);
cdc::serial::Console::printf("Username: %s\r\n", entry.username);
cdc::serial::Console::printf("Password: %s\r\n", entry.password);  // PLAINTEXT PASSWORD
cdc::serial::Console::printf("URL: %s\r\n", entry.url);
cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
```

Command registration at line 332:
```cpp
reg.registerCommand({"PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true});
```

## Recommended Fix
1. **Mask the password field** in the output, showing only a placeholder like `****` or first/last characters
2. **Add an optional flag** to show the full password (e.g., `PASSWORD_GET <index> VERBOSE`) that requires explicit user confirmation
3. **Consider authentication** - ensure the command requires PIN authentication before revealing sensitive fields

Example fix:
```cpp
cdc::serial::Console::printf("Password: **** (use PASSWORD_GET <index> VERBOSE to show)\r\n");
```

## References
- OWASP: [Secrets Management](https://owasp.org/www-project-cheat-sheets/cheatsheets/Secrets_Management_Cheat_Sheet.html)
- CTAP2/FIDO2 specifications typically mask PINs and credentials in logs and responses
