---
title: "[LOW] FIDO2 CTAP2 response bytes logged as hex dumps in debug mode"
severity: LOW
domain: fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The FIDO2 CTAP2 implementation logs raw response bytes as hex dumps for debugging purposes. These hex dumps are logged at DEBUG level and expose the internal structure of CTAP2 responses including `getInfo` and `makeCredential` responses.

**Locations:**
- `components/mod_fido2/src/ctap2.cpp:634-641` - getInfo response
- `components/mod_fido2/src/ctap2.cpp:1157-1164` - makeCredential response

## Impact
- **Protocol fingerprinting**: Reveals exact response structure and field ordering
- **Implementation details**: Exposes internal buffer handling and parsing logic
- **Debug leakage**: If DEBUG_MODE is enabled in production, sensitive response data is logged
- **Reconnaissance aid**: Helps attackers understand the exact protocol implementation

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:634-641`
```cpp
LOG_I("CTAP2", "getInfo response len=%u", *response_len);
char hex[50] = {0};
for (size_t i = 0; i < *response_len && i < 15; i++)
    sprintf(hex + (i * 3), "%02X ", response[offset + i]);
LOG_D("CTAP2", "%03u: %s", offset, hex);
```

File: `components/mod_fido2/src/ctap2.cpp:1157-1164`
```cpp
LOG_I("CTAP2", "makeCredential status=0x%02X resp_len=%u", status, *response_len);
char hex[50] = {0};
for (size_t i = 0; i < *response_len && i < 15; i++)
    sprintf(hex + (i * 3), "%02X ", response[offset + i]);
LOG_D("CTAP2", "%03u: %s", offset, hex);
```

## Recommended Fix
1. Remove hex dump logging or wrap in stricter debug conditionals
2. If hex dumps are needed, log only first few bytes for identification
3. Consider using a dedicated debug trace macro instead of LOG_D

Example fix:
```cpp
// Remove hex dump, keep only summary:
LOG_I("CTAP2", "getInfo response len=%u", *response_len);
// Or use conditional debug:
#ifdef CTAP2_DEBUG_VERBOSE
LOG_D("CTAP2", "%03u: %s", offset, hex);
#endif
```

## References
- CTAP2 specification: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html
- Related to issue #20 (CTAPHID packet logging)
- Related to issue #4 (makeCredential response logging)
