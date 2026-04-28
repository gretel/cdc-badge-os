---
title: "[LOW] FIDO2 pinUvAuthParam HMAC logged in DEBUG_MODE"
severity: LOW
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, when `DEBUG_MODE` is enabled, the received `pinUvAuthParam` (HMAC of the command) is logged at lines 1559-1570 for debugging.

## Impact
- **Authentication Token Exposure**: The HMAC is the authentication token for FIDO2 commands; logging it aids replay attacks
- **Command Structure Info**: The expected HMAC is also logged, revealing how the authentication works
- **DEBUG_MODE Default**: `DEBUG_MODE` defaults to 1, making this exposure common

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:1559-1570`

```cpp
#if DEBUG_MODE
    LOG_D("CTAP2", "pinUvAuthParam received (%zu bytes):", p->pin_uv_auth_param_len);
    LOG_D("CTAP2", "  %02X%02X%02X%02X %02X%02X%02X%02X...",
          p->pin_uv_auth_param[0], p->pin_uv_auth_param[1],
          p->pin_uv_auth_param[2], p->pin_uv_auth_param[3],
          p->pin_uv_auth_param[4], p->pin_uv_auth_param[5],
          p->pin_uv_auth_param[6], p->pin_uv_auth_param[7]);
    LOG_D("CTAP2", "Expected HMAC (first %zu bytes):", compare_len);
    LOG_D("CTAP2", "  %02X%02X%02X%02X %02X%02X%02X%02X...",
          expected_hmac[0], expected_hmac[1], expected_hmac[2], expected_hmac[3],
          expected_hmac[4], expected_hmac[5], expected_hmac[6], expected_hmac[7]);
#endif
```

## Recommended Fix
1. **Remove pinUvAuthParam logging** entirely or log only a hash of it
2. **Add `AUTH_DEBUG` flag** separate from general `DEBUG_MODE`

Example fix:
```cpp
#if DEBUG_MODE
    // Log only that auth param was received, not the value
    LOG_D("CTAP2", "pinUvAuthParam received (%zu bytes)", p->pin_uv_auth_param_len);
#endif
```

## References
- FIDO2 CTAP2 specification: Client PIN protocol
- CTAP2 command authentication using pinUvAuthParam

</content>