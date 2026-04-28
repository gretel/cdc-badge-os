---
title: "[MEDIUM] FIDO2 PIN hash and platform key exposed in DEBUG_MODE logs"
severity: MEDIUM
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, when `DEBUG_MODE` is enabled, the FIDO2 command processing logs the decrypted PIN hash and the full platform public key (used for PIN authentication) to the serial console. This occurs at lines 2463-2559.

## Impact
- **PIN Hash Exposure**: The decrypted 16-byte PIN hash is logged, which could be used for offline brute-force attacks
- **Platform Key Exposure**: The full 64-byte platform public key (X and Y coordinates) is logged in hex format
- **Shared Secret Risk**: While the shared secret itself isn't directly logged, the components needed to derive it are exposed
- **Debug Mode Prevalence**: `DEBUG_MODE` is commonly enabled during development and may be accidentally left on in production builds

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:2463-2510`

Lines 2463-2475 (PIN hash logging):
```cpp
#if DEBUG_MODE
    // Log received pinHashEnc for debugging
    LOG_I("PIN", "Received pinHashEnc (%zu bytes):", pin_hash_enc_len);
    for (size_t i = 0; i < pin_hash_enc_len; i += 16) {
        size_t row_len = (pin_hash_enc_len - i < 16) ? (pin_hash_enc_len - i) : 16;
        char hex[64];
        char *p = hex;
        for (size_t j = 0; j < row_len; j++) {
            p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
        }
        LOG_I("PIN", "  %s", hex);
    }
#endif
```

Lines 2485-2509 (Platform key logging):
```cpp
#if DEBUG_MODE
    // Full platform key for verification
    LOG_I("PIN", "Platform (Chrome) public key X:");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          platform_key_x[0], platform_key_x[1], ...);
    LOG_I("PIN", "Platform (Chrome) public key Y:");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          platform_key_y[0], platform_key_y[1], ...);
#endif
```

Lines 2545-2559 (Decrypted PIN hash):
```cpp
#if DEBUG_MODE
    LOG_D("PIN", "Decrypted PIN hash: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          decrypted_pin_hash[0], decrypted_pin_hash[1], ...);
    ...
#endif
```

## Recommended Fix
1. **Remove PIN hash logging** entirely or use a non-reversible representation (e.g., hash of the hash)
2. **Limit platform key logging** to first few bytes with ellipsis or use a checksum
3. **Add a stricter debug flag** like `PIN_DEBUG` that's separate from general `DEBUG_MODE`
4. **Ensure DEBUG_MODE is disabled** in production builds via build configuration

Example fix:
```cpp
#if DEBUG_MODE
    // Log only first 4 bytes of PIN hash for debugging
    LOG_I("PIN", "PIN hash (first 4 bytes): %02X%02X%02X%02X...",
          decrypted_pin_hash[0], decrypted_pin_hash[1], 
          decrypted_pin_hash[2], decrypted_pin_hash[3]);
#endif
```

## References
- FIDO2 CTAP2 specification: ClientPIN protocol
- OWASP: [Logging Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)
