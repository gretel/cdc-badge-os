---
title: "[LOW] FIDO2 ECDH public key coordinates logged in DEBUG_MODE"
severity: LOW
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, when `DEBUG_MODE` is enabled, the device's ECDH public key coordinates (X and Y) are logged at lines 2279-2285 during key agreement.

## Impact
- **Key Material Exposure**: While public keys are not secret, logging them aids reconnaissance
- **Replay Attack Info**: Combined with platform public key logs, helps attackers understand the ECDH exchange
- **DEBUG_MODE Default**: `DEBUG_MODE` defaults to 1, making this exposure common

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:2279-2285`

```cpp
#if DEBUG_MODE
    LOG_I("PIN", "=== Our ECDH public key ===");
    LOG_I("PIN", "X: %02X%02X%02X%02X %02X%02X%02X%02X...",
          pub_x[0], pub_x[1], pub_x[2], pub_x[3], pub_x[4], pub_x[5], pub_x[6], pub_x[7]);
    LOG_I("PIN", "Y: %02X%02X%02X%02X %02X%02X%02X%02X...",
          pub_y[0], pub_y[1], pub_y[2], pub_y[3], pub_y[4], pub_y[5], pub_y[6], pub_y[7]);
#endif
```

## Recommended Fix
1. **Remove public key logging** entirely or log only a hash/shortened version
2. **Add `KEY_DEBUG` flag** separate from general `DEBUG_MODE`

Example fix:
```cpp
#if DEBUG_MODE
    // Log only first 4 bytes of each coordinate
    LOG_I("PIN", "ECDH public key X (first 4): %02X%02X%02X%02X...",
          pub_x[0], pub_x[1], pub_x[2], pub_x[3]);
    LOG_I("PIN", "ECDH public key Y (first 4): %02X%02X%02X%02X...",
          pub_y[0], pub_y[1], pub_y[2], pub_y[3]);
#endif
```

## References
- FIDO2 CTAP2 specification: Client PIN protocol

</content>