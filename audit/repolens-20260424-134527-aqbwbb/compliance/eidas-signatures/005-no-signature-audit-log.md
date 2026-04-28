---
title: "[LOW] Signature operations not logged for non-repudiation audit trail"
severity: LOW
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "audit-trail"
  - "logging"
---

## Summary
Signature operations in FIDO2 and GPG modules are not logged with sufficient detail for a non-repudiation audit trail. While the code uses `cdc_log` for debug output, there's no persistent audit log of signature events with timestamps and context.

**Evidence:**
- File: `components/mod_fido2/src/ctap2.cpp` - FIDO2 signing operations log at DEBUG level only
- File: `components/mod_gpg/src/gpg.cpp:577-595` - `gpg_sign_hash()` has no logging
- File: `components/cdc_core/src/AttestationKeyService.cpp` - Attestation key operations logged but not signature events
- No persistent audit log storage in NVS or R-Memory

## Impact
For eIDAS 2.0 compliance:
1. **Non-repudiation** requires an audit trail of signature operations
2. **Accountability**: Users should be able to review what was signed and when
3. **Forensics**: In case of dispute, signature events should be traceable
4. **Compliance**: eIDAS Article 26 requires signature creation data to be under sole control, auditable

Current implementation logs at DEBUG level which may not persist. No audit query interface exists.

## Evidence
```cpp
// components/mod_gpg/src/gpg.cpp:577-595
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len) {
    if (!hash || !sig_out || !sig_len) return false;
    if (!s_initialized) return false;
    // No logging of signature event!
    if (s_metadata.curve == CDC_CURVE_P256) {
        return se_sign_p256(gpg_storage_sig_slot(), hash, hash_len, sig_out, sig_len);
    }
}
```

```cpp
// components/mod_fido2/src/ctap2.cpp - DEBUG level logging only
LOG_D("CTAP2", "Signing assertion...");
```

## Recommended Fix
1. Add audit logging to signature operations:
   - Timestamp (Unix time)
   - Slot/key used
   - Hash of data signed (first 16 bytes)
   - Signature count at time of signing
2. Store audit log in NVS or R-Memory (circular buffer, last 100 signatures)
3. Add serial command: `AUDIT_LOG [count]` to retrieve recent signature events

**Scope:** Add audit logging to GPG and FIDO2 signing functions (one-hour task)

## References
- [eIDAS Regulation (EU) No 910/2014](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32014R0910) - Article 26 (Qualified signature creation devices)
- [FIDO2 Authentication Count](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-v2.0-rd-20180702.html#authenticator-parameters) - Counter mechanism
