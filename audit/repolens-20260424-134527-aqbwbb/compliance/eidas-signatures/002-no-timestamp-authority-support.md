---
title: "[MEDIUM] No timestamp authority (TSA) support for signature non-repudiation"
severity: MEDIUM
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "timestamp"
  - "non-repudiation"
---

## Summary
The signature operations in FIDO2 (`components/mod_fido2/src/`) and GPG (`components/mod_gpg/src/gpg.cpp:577-595`) do not include timestamp authority (TSA) integration for RFC 3161 timestamps. Signatures lack trusted timestamps for non-repudiation.

**Evidence:**
- File: `components/mod_gpg/src/gpg.cpp:577-595` - `gpg_sign_hash()` signs data without timestamp
- File: `components/mod_fido2/src/ctap2.cpp` - FIDO2 assertions include counter but no trusted timestamp
- No RFC 3161 timestamp client implementation

## Impact
For eIDAS 2.0 compliance:
1. **Qualified Electronic Signatures (QES)** require trusted timestamps for long-term validity
2. **Non-repudiation** requires proof of when a signature was created
3. **Signature expiry**: Without timestamps, signatures cannot be validated after key expiry
4. **Legal effect**: eIDAS Article 26 requires time-stamping for qualified signatures

Current implementation stores a `sign_count` counter but no trusted wall-clock timestamp.

## Evidence
```cpp
// components/mod_gpg/src/gpg.cpp:577-595
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len) {
    if (!hash || !sig_out || !sig_len) return false;
    if (!s_initialized) return false;

    if (s_metadata.curve == CDC_CURVE_P256) {
        return se_sign_p256(gpg_storage_sig_slot(), hash, hash_len, sig_out, sig_len);
    }
    // ... Ed25519 signing
}
// No timestamp included in signature structure
```

```cpp
// components/mod_fido2/src/ctap2.cpp - FIDO2 assertion includes counter only
// Counter increments but no trusted timestamp
```

## Recommended Fix
1. Add RFC 3161 timestamp client to request timestamps from a TSA after signing
2. Store timestamp alongside signature metadata in NVS
3. Add serial command: `SIGN_WITH_TIMESTAMP <slot> <hash>` that returns signature + TSA timestamp
4. Document TSA configuration (at least one qualified TSA URL)

**Scope:** Implement basic RFC 3161 client and configurable TSA endpoint (one-hour task)

## References
- [eIDAS Regulation (EU) No 910/2014](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32014R0910) - Article 3 (Definitions: qualified timestamp)
- [RFC 3161](https://datatracker.ietf.org/doc/html/rfc3161) - Internet X.509 Public Key Infrastructure Time-Stamp Protocol
- [MbedTLS TSA example](https://github.com/Mbed-TLS/mbedtls/tree/development/programs/pk)
