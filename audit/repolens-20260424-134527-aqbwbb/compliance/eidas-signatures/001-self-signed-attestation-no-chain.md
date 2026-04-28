---
title: "[MEDIUM] Self-signed attestation certificate lacks qualified certificate chain"
severity: MEDIUM
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "certificate-management"
  - "attestation"
---

## Summary
The FIDO2/U2F module generates self-signed attestation certificates in `components/mod_fido2/src/u2f.cpp:111-318`. The certificate is built manually with DER encoding but is **self-signed** (issuer = subject) and not part of a qualified certificate chain from a Trust Service Provider (TSP).

**Evidence:**
- File: `components/mod_fido2/src/u2f.cpp:111-318`
- Function: `u2f_init_attestation()`
- The certificate is built manually with hardcoded issuer/subject (CDC Badge FIDO2) and signed with the same key stored in ECC slot 0

## Impact
For eIDAS 2.0 compliance, electronic signature devices require:
1. **Qualified certificates** issued by a Qualified Trust Service Provider (QTSP)
2. **Certificate chain validation** up to a trusted root
3. **Attestation hierarchy** for FIDO2 to be recognized as a qualified signature creation device

Self-signed certificates work for FIDO2 basic authentication but do not meet eIDAS requirements for:
- Qualified Electronic Signatures (QES)
- Electronic Identification (eID) with legal effect
- Cross-border recognition under eIDAS 2.0

## Evidence
```cpp
// components/mod_fido2/src/u2f.cpp:155-208
// FIDO2-konformer Subject: C=DE, O=CDC, OU=Authenticator Attestation, CN=CDC Badge FIDO2
static const uint8_t fido2_subject[] = {
    0x30, 0x59,  // SEQUENCE (89 bytes)
    // C=DE (13 bytes)
    0x31, 0x0B, 0x30, 0x09,
    0x06, 0x03, 0x55, 0x04, 0x06,  // OID: C (2.5.4.6)
    0x13, 0x02, 'D', 'E',
    // O=CDC (14 bytes)
    0x31, 0x0C, 0x30, 0x0A,
    0x06, 0x03, 0x55, 0x04, 0x0A,  // OID: O (2.5.4.10)
    0x0C, 0x03, 'C', 'D', 'C',
    // ... more hardcoded fields
};
// Self-signed: TBS is signed with the same key (slot 0) that is the subject
```

## Recommended Fix
1. Add support for loading an external attestation certificate from NVS or secure storage
2. Implement certificate chain validation using MbedTLS X.509 module
3. Document how to provision qualified certificates from a QTSP
4. Add serial command to import attestation certificate: `ATTEST_IMPORT <cert_der>`

**Scope:** Add certificate import and chain validation capability (not full QTSP integration, which is external)

## References
- [eIDAS Regulation (EU) No 910/2014](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32014R0910) - Article 26 (Qualified signature creation devices)
- [FIDO2 Attestation](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-v2.0-rd-20180702.html#attestation) - Basic vs. Attestation
- [MbedTLS X.509 module](https://www.mbed-tls.com/documentation/3.5.0/x509_8h.html)
