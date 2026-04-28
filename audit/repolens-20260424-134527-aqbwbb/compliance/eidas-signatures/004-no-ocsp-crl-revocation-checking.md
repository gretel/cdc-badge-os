---
title: "[MEDIUM] No OCSP/CRL revocation checking for certificate validation"
severity: MEDIUM
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "certificate-validation"
  - "revocation"
---

## Summary
The codebase has no implementation of certificate revocation checking via OCSP (Online Certificate Status Protocol) or CRL (Certificate Revocation List). When certificates are used (e.g., FIDO2 attestation), there is no mechanism to verify they haven't been revoked.

**Evidence:**
- File: `components/mod_fido2/src/u2f.cpp` - Attestation certificate is generated and cached but never validated against revocation
- File: `components/cdc_hal/src/Tropic01Element.cpp` - No certificate validation logic
- No OCSP client or CRL parser implementation
- No MbedTLS X.509 verification with revocation checking

## Impact
For eIDAS 2.0 compliance:
1. **Certificate validation** requires checking revocation status (eIDAS Article 24)
2. **Long-term signature validation (LTV)** requires OCSP/CRL data embedded in signature
3. **Trust chain**: Without revocation checking, revoked certificates can still validate signatures
4. **Legal effect**: Signatures validated without revocation checking may not have legal effect

Current implementation assumes all self-signed attestation certificates are valid indefinitely.

## Evidence
```cpp
// components/mod_fido2/src/u2f.cpp:111-318
// Certificate is generated and cached, but never validated
bool u2f_init_attestation(void) {
    // ... builds self-signed certificate ...
    g_attest_initialized = true;
    // No OCSP/CRL checking
}

// components/mod_fido2/src/u2f.cpp:331-338
bool u2f_get_attestation_cert(const uint8_t **cert, uint16_t *cert_len) {
    // Returns cached certificate without validation
}
```

No code exists for:
- OCSP request/response parsing
- CRL download and parsing
- Certificate chain validation with revocation

## Recommended Fix
1. Add OCSP client using MbedTLS (`mbedtls_x509_crt_verify_with_ocsp()`)
2. Add serial command: `VERIFY_CERT <cert_der>` that checks:
   - Certificate chain validity
   - OCSP status (configurable TSA URL)
   - CRL if OCSP unavailable
3. Document how to configure OCSP responder URL for attestation certificates

**Scope:** Implement basic OCSP client and certificate verification command (one-hour task)

## References
- [eIDAS Regulation (EU) No 910/2014](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32014R0910) - Article 24 (Validation of qualified certificates)
- [RFC 6960](https://datatracker.ietf.org/doc/html/rfc6960) - OCSP protocol
- [MbedTLS X.509 verification](https://www.mbed-tls.com/documentation/3.5.0/x509_crt_8h.html) - Certificate chain validation
