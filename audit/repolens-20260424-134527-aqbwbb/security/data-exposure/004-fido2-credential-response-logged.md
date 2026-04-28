---
title: "[MEDIUM] FIDO2 makeCredential response dumped to debug logs"
severity: MEDIUM
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, the `ctap2_make_credential` function dumps the full CBOR response (including credential ID, public key, and attestation data) to debug logs when `CTAP2_DEBUG` is enabled. This occurs at lines 1156-1166.

## Impact
- **Credential ID Exposure**: The credential ID (often containing user handle and key reference) is logged in hex
- **Public Key Exposure**: The generated public key is logged, which while not secret, reveals the key structure
- **Attestation Data**: Full attestation certificate data is logged
- **Reconnaissance Aid**: Logs can help attackers understand the key structure and format for replay attacks

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:1156-1166`

```cpp
#if CTAP2_DEBUG
    LOG_I("CTAP2", "makeCredential status=0x%02X resp_len=%u", status, *response_len);
    for (uint16_t offset = 0; offset < *response_len; offset += 16) {
        char hex[50] = {0};
        int dump_len = ((*response_len - offset) < 16) ? (*response_len - offset) : 16;
        for (int i = 0; i < dump_len; i++) {
            sprintf(hex + (i * 3), "%02X ", response[offset + i]);
        }
        LOG_D("CTAP2", "%03u: %s", offset, hex);
    }
#endif
```

Similar pattern exists in `ctap2_get_info` at lines 633-643.

## Recommended Fix
1. **Remove full response dumps** or limit to first/last few bytes
2. **Log only metadata** like response length and status code
3. **Add a more specific flag** like `VERBOSE_CBOR` that's separate from `CTAP2_DEBUG`

Example fix:
```cpp
#if CTAP2_DEBUG
    // Log only summary, not full response
    LOG_I("CTAP2", "makeCredential status=0x%02X resp_len=%u", status, *response_len);
    // Full dump removed for security
#endif
```

## References
- FIDO2 CTAP2 specification
- OWASP: [Logging Cheat Sheet - Sensitive Data](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html#do-not-log-sensitive-information)
