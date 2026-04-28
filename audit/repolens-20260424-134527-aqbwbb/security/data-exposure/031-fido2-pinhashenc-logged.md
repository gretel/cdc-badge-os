---
title: "[MEDIUM] FIDO2 pinHashEnc (encrypted PIN hash) logged to serial output"
severity: MEDIUM
domain: fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The FIDO2 CTAP2 PIN protocol implementation logs the encrypted PIN hash (`pinHashEnc`) to the serial console, exposing cryptographic material that should remain internal. This occurs in the `getPinUvAuthParam` function where the received encrypted PIN hash is logged for debugging purposes.

**Location:** `components/mod_fido2/src/ctap2.cpp:2465-2473`

## Impact
- **Cryptographic material exposure**: The `pinHashEnc` is an encrypted representation of the PIN hash, used for authentication
- **Side-channel attack vector**: Logging this value reveals the exact bytes being processed during PIN verification
- **Debug info leakage**: While logged at INFO level, this can be captured in production logs if DEBUG_MODE is enabled
- **Protocol structure exposure**: The logging reveals the internal structure of the CTAP2 PIN protocol

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:2465-2473`
```cpp
LOG_I("PIN", "Received pinHashEnc (%zu bytes):", pin_hash_enc_len);
for (size_t i = 0; i < pin_hash_enc_len; i += 16) {
    size_t row_len = (pin_hash_enc_len - i < 16) ? (pin_hash_enc_len - i) : 16;
    char hex[64];
    char *p = hex;
    for (size_t j = 0; j < row_len; j++)
        p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
    LOG_I("PIN", "  %s", hex);
}
```

The `pinHashEnc` is the encrypted PIN hash used in CTAP2 PIN protocol (pinProtocol 2). It contains:
- First 16 bytes: IV (initialization vector)
- Remaining bytes: Ciphertext of the PIN hash

## Recommended Fix
1. Remove the hex dump logging of `pinHashEnc` or reduce it to a single summary line
2. If debugging is needed, log only the length and a checksum, not the full bytes
3. Consider using a debug-only macro to wrap the detailed logging

Example fix:
```cpp
// Instead of logging full hex dump:
LOG_I("PIN", "Received pinHashEnc (%zu bytes)", pin_hash_enc_len);
// Or use debug-only logging:
#ifdef DEBUG_MODE
LOG_D("PIN", "Received pinHashEnc (%zu bytes)", pin_hash_enc_len);
#endif
```

## References
- CTAP2 PIN Protocol: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#pin-protocols
- Related to issue #13 (pinUvAuthParam logging)
- Related to issue #3 (PIN hash in DEBUG_MODE)
