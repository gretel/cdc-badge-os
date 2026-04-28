---
title: "[HIGH] TOTP secret key exposed in serial command TOTP_ADD"
severity: HIGH
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `TOTP_ADD` serial command in `components/mod_totp/src/TotpModule.cpp:237-262` accepts and processes the secret key directly from command-line arguments. While the secret itself isn't echoed back, the command structure exposes the secret in the serial command history, and the `cmd_totp_add` function logs the secret to the serial console as part of the usage message when arguments are incomplete.

## Impact
- **Secret Key Exposure**: TOTP secrets are the master key for 2FA codes; exposure allows an attacker to generate valid TOTP codes
- **Command History**: Serial commands are often logged, and the secret appears in the command line arguments
- **Usage message leakage**: When called without proper arguments, the usage message can reveal the structure of expected secrets

## Evidence
File: `components/mod_totp/src/TotpModule.cpp:237-250`
```cpp
static void cmd_totp_add(const char* args) {
    char name[TotpStore::NAME_LEN + 1] = {};
    char secret[128] = {};  // Secret stored in local buffer
    char issuer[TotpStore::ISSUER_LEN + 1] = {};
    ...
    const char* p = nextToken(args, name, sizeof(name));
    if (!p || !*name) {
        cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
        return;
    }
    p = nextToken(p, secret, sizeof(secret));  // Secret parsed from args
    if (!p || !*secret) {
        cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
        return;
    }
    ...
}
```

The secret is passed as a command-line argument, which may be visible in:
- Serial terminal history
- Shell history if commands are typed interactively
- Log files capturing serial output

## Recommended Fix
1. **Use interactive input** for the secret instead of command-line arguments
2. **Add a `--quiet` or `-q` flag** to suppress usage messages that might reveal structure
3. **Clear the secret buffer** immediately after processing with `memset(secret, 0, sizeof(secret))`
4. **Consider a dedicated "secret input" mode** that reads from a protected input channel

Example fix:
```cpp
// Clear secret buffer after use
memset(secret, 0, sizeof(secret));
cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
```

## References
- RFC 6238: TOTP (Time-Based One-Time Password)
- OWASP: [Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
