---
title: "[LOW] TOTP secret printed in plaintext via serial command"
severity: LOW
domain: secrets
lens: secrets-credential-management
labels:
  - "audit:security/secrets"
---

## Summary
The TOTP module has a serial command `TOTP_SHOW_SECRET` that prints the Base32-encoded secret for a TOTP account. This is in `components/mod_totp/src/TotpModule.cpp:561`.

## Impact
**Security Risk**: The TOTP secret is the root of trust for time-based codes. If exposed:
- Anyone can generate valid TOTP codes
- Two-factor authentication is compromised
- Serial logs may retain the secret

This is less critical than password exposure since TOTP secrets are typically used for 2FA, but still valuable.

## Evidence

**File: `components/mod_totp/src/TotpModule.cpp`**
```cpp
// Lines 555-580 - TOTP_SHOW_SECRET command
static void cmd_totp_show_secret(const char* args) {
    char nameBuf[64] = {};
    const char* p = nextToken(args, nameBuf, sizeof(nameBuf));
    if (!p || !*nameBuf) {
        cdc::serial::Console::printf("Usage: TOTP_SHOW_SECRET <name>\r\n");
        return;
    }

    TotpStore::Account acc = {};
    if (!TotpStore::instance().findAccountByName(nameBuf, &acc)) {
        cdc::serial::Console::printf("Account not found: %s\r\n", nameBuf);
        return;
    }

    char secret[128];
    base32Encode(acc.secret, acc.secretLen, secret, sizeof(secret));
    cdc::serial::Console::printf("Secret: %s\r\n", secret);  // <-- PLAINTEXT SECRET
}
```

**File: `components/mod_totp/src/TotpModule.cpp`**
```cpp
// Lines 224-265 - TOTP_ADD command also prints the secret
static void cmd_totp_add(const char* args) {
    char secret[128] = {};
    ...
    p = nextToken(p, secret, sizeof(secret));
    ...
    // Later in the function:
    cdc::serial::Console::printf("Account added: %s\r\n", name);
    // The secret is available in the 'secret' buffer and could be logged
}
```

## Recommended Fix
1. **Add masking option**: Show truncated secret by default:
   ```
   TOTP_SHOW_SECRET <name>        // Shows: "JBSWY3D***" (first 6 chars + stars)
   TOTP_SHOW_SECRET <name> --raw  // Shows full secret (opt-in)
   ```

2. **Add confirmation prompt**: For the raw secret, require a second confirmation:
   ```
   TOTP_SHOW_SECRET <name> --raw
   // Prompt: "Secret will be shown. Type CONFIRM to continue: "
   ```

3. **Clear secret buffer**: Ensure the secret buffer is cleared after printing:
   ```cpp
   char secret[128];
   base32Encode(acc.secret, acc.secretLen, secret, sizeof(secret));
   cdc::serial::Console::printf("Secret: %s\r\n", secret);
   mbedtls_platform_zeroize(secret, sizeof(secret));
   ```

## References
- [TOTP RFC 6238](https://tools.ietf.org/html/rfc6238) - Time-based One-Time Password algorithm
- [HOTP RFC 4226](https://tools.ietf.org/html/rfc4226) - HMAC-based One-Time Password
- [OWASP 2FA Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Two_Factor_Authentication_Cheat_Sheet.html)
