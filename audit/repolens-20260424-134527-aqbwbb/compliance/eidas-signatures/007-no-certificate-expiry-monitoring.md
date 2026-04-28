---
title: "[LOW] No certificate expiry monitoring or renewal mechanism"
severity: LOW
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "certificate-management"
  - "expiry"
---

## Summary
The codebase has no mechanism to monitor certificate expiry or trigger renewal. The self-signed attestation certificate in FIDO2 has a hardcoded validity period (2024-01-01 to 2049-12-31) but there's no code to check expiry or notify when certificates approach expiration.

**Evidence:**
- File: `components/mod_fido2/src/u2f.cpp:216-218` - Certificate validity hardcoded to 2049-12-31
- File: `components/mod_gpg/src/openpgp/openpgp.cpp` - No certificate expiry tracking
- No code for:
  - Reading `notAfter` field from certificates
  - Comparing against current time
  - Alerting when certificate approaches expiry
  - Certificate renewal workflow

## Impact
For eIDAS 2.0 compliance:
1. **Qualified certificates** have defined validity periods (typically 1-5 years)
2. **Signature validity**: Signatures made with expired certificates may not have legal effect
3. **Long-term validation (LTV)**: Requires knowledge of certificate expiry
4. **User awareness**: Users should be notified before certificates expire

Current implementation assumes certificates are valid indefinitely or uses hardcoded dates without checking.

## Evidence
```cpp
// components/mod_fido2/src/u2f.cpp:216-218
// Hardcoded validity - no dynamic checking
static const uint8_t validity[] = {
    0x30, 0x1E,  // SEQUENCE
    0x17, 0x0D, '2', '4', '0', '1', '0', '1', '0', '0', '0', '0', '0', '0', 'Z',  // notBefore: 2024-01-01
    0x17, 0x0D, '4', '9', '1', '2', '3', '1', '2', '3', '5', '9', '5', '9', 'Z'   // notAfter:  2049-12-31
};
```

No code exists for:
- Parsing X.509 validity dates
- Comparing certificate expiry with current time
- Alerting when certificate is near expiry (e.g., 30 days before)
- Certificate renewal workflow

## Recommended Fix
1. Add function to parse X.509 validity dates from DER certificate
2. Add function to check if certificate is expired or approaching expiry
3. Add serial command: `CERT_STATUS` that reports:
   - Whether attestation certificate exists
   - Expiry date
   - Days until expiry (or "expired")
4. Add simple expiry check on boot with INFO log message

**Scope:** Add basic certificate expiry checking and status command (one-hour task)

## References
- [eIDAS Regulation (EU) No 910/2014](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32014R0910) - Article 24 (Validation of qualified certificates)
- [X.509 Certificate Validity](https://datatracker.ietf.org/doc/html/rfc5280#section-4.1.2.5) - RFC 5280 Section 4.1.2.5
- [MbedTLS X.509](https://www.mbed-tls.com/documentation/3.5.0/x509_crt_8h.html) - Certificate parsing

</content>