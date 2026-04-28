---
title: "[MEDIUM] Missing curve parameter validation in GPG_GENERATE serial command"
severity: MEDIUM
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `cmd_gpg_generate` function in `components/mod_gpg/src/GpgModule.cpp` accepts a curve parameter via the serial command `GPG_GENERATE` but does not validate that the input is a valid curve identifier. The code uses `atoi()` to parse the curve parameter and defaults to Ed25519 for any non-2 value, without providing feedback to the user about what values are valid.

**Location:** `components/mod_gpg/src/GpgModule.cpp:150-179`

## Impact
- **Silent fallback**: Any input that isn't exactly "2" defaults to Ed25519 without warning. Users might think they're selecting P-256 (curve=2) but get Ed25519 instead.
- **Invalid values accepted**: Values like "3", "10", "100", or even non-numeric strings like "foo" all silently default to Ed25519
- **Confusion**: Users may not realize their curve selection didn't work as expected
- **No error feedback**: The command returns "OK" even when an invalid curve value was provided

## Evidence
```cpp
// components/mod_gpg/src/GpgModule.cpp:150-179
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... skip whitespace, parse curveBuf ...
    
    // Parse user_id
    strncpy(userId, p, sizeof(userId) - 1);

    // No validation - defaults to Ed25519 for anything != 2
    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The valid curve constants are defined in `components/mod_gpg/include/mod_gpg/gpg.h`:
```cpp
#define CDC_CURVE_ED25519 0
#define CDC_CURVE_P256    1
```

But the command accepts any integer and silently maps non-2 values to Ed25519.

For comparison, the TOTP module at least validates the algorithm parameter with named options:
```cpp
// mod_totp/src/TotpModule.cpp:136-147
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    char buf[8] = {};
    // ... parse to lowercase ...
    if (strcmp(buf, "sha1") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    if (strcmp(buf, "sha256") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA256);
    if (strcmp(buf, "sha512") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA512);
    return static_cast<uint8_t>(atoi(buf));  // Falls back to numeric
}
```

## Recommended Fix
Add explicit validation for the curve parameter with clear error messages:

```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... skip whitespace, parse curveBuf ...
    
    // Parse user_id
    strncpy(userId, p, sizeof(userId) - 1);

    // Validate curve parameter
    uint8_t curve;
    if (strcmp(curveBuf, "0") == 0 || strcasecmp(curveBuf, "ed25519") == 0) {
        curve = CDC_CURVE_ED25519;
    } else if (strcmp(curveBuf, "1") == 0 || strcasecmp(curveBuf, "p256") == 0 || 
               strcmp(curveBuf, "2") == 0) {  // 1 or 2 both map to P-256
        curve = CDC_CURVE_P256;
    } else {
        cdc::serial::Console::printf("ERROR: Invalid curve '%s'. Use 0 (Ed25519) or 1/2 (P-256)\r\n", curveBuf);
        return;
    }
    
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Alternatively, support both numeric and text-based curve names for better usability:
- `0` or `ed25519` → Ed25519
- `1` or `p256` → P-256

## References
- CWE-20: Improper Input Validation
- CWE-252: Unchecked Return Value (related to silent fallback)
- OpenPGP Smart Card specification for curve selection
