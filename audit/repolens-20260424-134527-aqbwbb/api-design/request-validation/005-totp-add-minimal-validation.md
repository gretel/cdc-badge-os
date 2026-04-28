---
title: "[MEDIUM] TOTP_ADD command lacks name and secret length validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `TOTP_ADD` serial command in `components/mod_totp/src/TotpModule.cpp:224-248` validates that name and secret fields are non-empty but doesn't validate maximum lengths or that the secret is valid base32-encoded data.

**File**: `components/mod_totp/src/TotpModule.cpp`  
**Lines**: 224-248  
**Function**: `cmd_totp_add()`

## Impact
- **Silent truncation**: Long names and secrets are truncated without warning
- **Invalid base32**: Non-base32 characters in the secret are passed to the store, potentially causing failures later
- **No validation of required formats**: Secrets should be valid base32 (A-Z, 2-7, padding)

## Evidence
From `components/mod_totp/src/TotpModule.cpp:224-252`:

```cpp
char name[TotpStore::NAME_LEN + 1] = {};
char secret[128] = {};
char issuer[TotpStore::ISSUER_LEN + 1] = {};

const char* p = nextToken(args, name, sizeof(name));
if (!p || !*name) {
    cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
    return;
}
p = nextToken(p, secret, sizeof(secret));
if (!p || !*secret) {
    cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
    return;
}
// ... no further validation ...
```

From `components/mod_totp/include/mod_totp/TotpStore.h:28-30`:
```cpp
static constexpr uint8_t NAME_LEN = 16;
static constexpr uint8_t ISSUER_LEN = 32;
static constexpr uint8_t SECRET_LEN = 32;  // Binary secret, not base32 string
```

The `nextToken()` function limits the input to the buffer size, but:
1. No validation that name fits `TotpStore::NAME_LEN` (16 chars)
2. No validation that secret is valid base32
3. No validation that secret length makes sense (typical secrets are 16-32 base32 chars)

Problematic inputs:
- `TOTP_ADD "VeryLongAccountNameThatExceeds16Chars" ABCDEF...` → name truncated to 16 chars
- `TOTP_ADD "Account" "ABC123"` → "123" are invalid base32 (should be A-Z, 2-7)
- `TOTP_ADD "A" "X"` → single character secret (probably too short)

## Recommended Fix
Add length and format validation:

```cpp
const char* p = nextToken(args, name, sizeof(name));
if (!p || !*name) {
    cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
    return;
}
// Validate name length
if (strlen(name) > TotpStore::NAME_LEN) {
    cdc::serial::Console::printf("ERROR: Name too long (max %d chars)\r\n", TotpStore::NAME_LEN);
    return;
}

p = nextToken(p, secret, sizeof(secret));
if (!p || !*secret) {
    cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
    return;
}
// Validate secret length (typical secrets are 16-32 base32 chars)
size_t secretLen = strlen(secret);
if (secretLen < 8 || secretLen > 64) {
    cdc::serial::Console::printf("ERROR: Secret must be 8-64 base32 characters\r\n");
    return;
}
// Validate base32 format (A-Z, 2-7, optionally = padding)
for (size_t i = 0; i < secretLen; i++) {
    char c = secret[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7') || (c == '='))) {
        cdc::serial::Console::printf("ERROR: Secret must be valid base32 (A-Z, 2-7, =)\r\n");
        return;
    }
}
```

## References
- RFC 4648 (Base32): https://datatracker.ietf.org/doc/html/rfc4648
- Typical TOTP secrets are 16-32 base32 characters (80-160 bits)
