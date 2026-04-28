---
title: "[LOW] FIDO2 attestation certificate lacks lifecycle management (no validity period, no rotation)"
severity: LOW
domain: encryption
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The FIDO2 module uses a self-signed attestation certificate without a defined validity period or rotation mechanism. The certificate is generated once during initialization and never expires or rotates. NIS2 requires "state-of-the-art" encryption and certificate management (Art. 20).

## Impact
**NIS2 Art. 20 Certificate Management Gap**:
- No certificate expiration means compromised keys remain valid indefinitely
- No rotation mechanism for key updates
- Self-signed certificate without chain of trust
- AAGUID is hardcoded, not device-unique per FIDO recommendations

## Evidence
1. **Self-signed certificate generated once at init**:
   - File: `components/mod_fido2/src/u2f.cpp:79`
   ```cpp
   bool u2f_init_attestation(void) {
       if (g_attest_initialized) {
           return true;  // Already initialized, never re-init
       }
       // ... builds self-signed certificate
   }
   ```
   - Certificate is cached in `g_attest_cert` and never refreshed

2. **No validity period in certificate**:
   - File: `components/mod_fido2/src/u2f.cpp:130-180`
   - Manual X.509 construction doesn't set `notBefore`/`notAfter` fields
   - Certificate is effectively valid forever

3. **No certificate rotation mechanism**:
   - No function to regenerate attestation certificate
   - No trigger for certificate renewal
   - No expiration check

4. **Hardcoded AAGUID**:
   - File: `components/mod_fido2/src/ctap2.cpp:15-24`
   ```cpp
   static const uint8_t AAGUID[16] = {
       0xCD, 0xCB, 0xAD, 0x6E,  // "CDCBAD6E"
       0x39, 0xC3,              // 39C3
       0x00, 0x01,              // Version 1
       0xBA, 0xD6, 0xE0, 0x01,  // "BADGE01"
       0x00, 0x00, 0x00, 0x01   // Device type
   };
   ```
   - Same AAGUID for all devices (should be unique per FIDO spec)

## Recommended Fix
1. **Add Certificate Validity Period**:
   - Set `notBefore` to current time during init
   - Set `notAfter` to 1-2 years from init
   - Store init timestamp in NVS for expiration check

2. **Implement Certificate Rotation**:
   - Add function to regenerate attestation key and certificate
   - Add serial command: `FIDO2 ROTATE_ATTESTATION`
   - Store rotation timestamp in NVS

3. **Add Expiration Check**:
   - Check certificate validity on device boot
   - Show warning if certificate is near expiration
   - Auto-generate new certificate if expired

4. **Make AAGUID Device-Unique**:
   - Use chip ID or unique device identifier to generate AAGUID
   - Follow FIDO specification for AAGUID format
   - Store in NVS for consistency

5. **Document Certificate Lifecycle**:
   - Add `docs/FIDO2_ATTESTATION.md` with certificate management details
   - Include rotation procedure
   - Document validity period and expiration handling

## References
- [FIDO2 Attestation Specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#attestation)
- [NIS2 Directive Art. 20 - Encryption](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-57 - Key Management](https://csrc.nist.gov/publications/detail/sp/800-57/part-1/final)
- [RFC 5280 - X.509 Certificate](https://datatracker.ietf.org/doc/html/rfc5280)
