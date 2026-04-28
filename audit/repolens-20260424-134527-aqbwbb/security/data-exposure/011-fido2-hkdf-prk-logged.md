---
title: "[LOW] FIDO2 HKDF PRK exposed in DEBUG_MODE logs"
severity: LOW
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, when `DEBUG_MODE` is enabled, the HKDF PRK (Pseudo-Random Key) derived during PIN authentication is logged at lines 2100-2109. This is an intermediate value in the shared secret derivation.

## Impact
- **Key Derivation Exposure**: The PRK is the output of HKDF Extract and input to HKDF Expand; exposure aids cryptographic analysis
- **Replay Attack Info**: Combined with other logged values, helps attackers understand the key derivation process
- **DEBUG_MODE Default**: `DEBUG_MODE` defaults to 1, making this exposure common in development

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:2100-2109`

```cpp
#if DEBUG_MODE
    LOG_I("PIN", "HKDF PRK (HMAC-SHA256(salt=0, IKM=Z)):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          prk[0], prk[1], prk[2], prk[3], prk[4], prk[5], prk[6], prk[7],
          prk[8], prk[9], prk[10], prk[11], prk[12], prk[13], prk[14], prk[15]);
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
          prk[16], prk[17], prk[18], prk[19], prk[20], prk[21], prk[22], prk[23],
          prk[24], prk[25], prk[26], prk[27], prk[28], prk[29], prk[30], prk[31]);
    LOG_I("PIN", "HKDF info: '%s' || 0x01 (len=%zu)", info, info_len + 1);
#endif
```

## Recommended Fix
1. **Remove PRK logging** entirely or log only first 4 bytes
2. **Add `HKDF_DEBUG` flag** separate from general `DEBUG_MODE`

Example fix:
```cpp
#if DEBUG_MODE
    // Log only first 4 bytes of PRK
    LOG_I("PIN", "HKDF PRK (first 4 bytes): %02X%02X%02X%02X...",
          prk[0], prk[1], prk[2], prk[3]);
#endif
```

## References
- RFC 5869: HKDF (HMAC-based Extract-and-Expand Key Derivation Function)
- CTAP2 specification: PIN protocol key derivation

</content>