---
title: "[MEDIUM] TOTP account names and secrets logged in serial commands"
severity: MEDIUM
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The TOTP module serial commands in `components/mod_totp/src/TotpModule.cpp` log account names and can expose TOTP secrets through the command-line interface. The `cmd_totp_list` function at line 208 outputs all account names with their slot numbers, and the `cmd_totp_get` function at line 311 outputs TOTP codes with issuer names.

## Impact
- **Account Enumeration**: All TOTP account names are listed, revealing which services the user has configured
- **Code Exposure**: TOTP codes are printed to serial, visible to anyone with USB-CDC access
- **Issuer Information**: Service names (issuer) are exposed, helping attackers understand which accounts are protected

## Evidence
File: `components/mod_totp/src/TotpModule.cpp:198-220`

Lines 208-210 (Account listing):
```cpp
cdc::serial::Console::printf("%u: %s (slot %u)\r\n", c->idx, entry.name, logical);
```

Lines 311-313 (TOTP code output):
```cpp
if (issuer && issuer[0]) {
    cdc::serial::Console::printf("%s (%ds) [%s]\r\n", code, remaining, issuer);
} else {
    cdc::serial::Console::printf("%s (%ds)\r\n", code, remaining);
}
```

## Recommended Fix
1. **Add authentication** requirement before listing or retrieving TOTP codes
2. **Mask account names** in list output (e.g., show first 3 characters)
3. **Add a verbose flag** to show full details only when explicitly requested

Example fix:
```cpp
// Show abbreviated account name in list
char abbreviated[10];
strncpy(abbreviated, entry.name, sizeof(abbreviated) - 1);
abbreviated[sizeof(abbreviated) - 1] = '\0';
cdc::serial::Console::printf("%u: %s... (slot %u)\r\n", c->idx, abbreviated, logical);
```

## References
- RFC 6238: TOTP specification
- OWASP: [Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
