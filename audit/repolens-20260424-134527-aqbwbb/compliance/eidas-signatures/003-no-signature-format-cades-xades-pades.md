---
title: "[MEDIUM] No support for qualified signature formats (CAdES, XAdES, PAdES)"
severity: MEDIUM
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "signature-format"
  - "eidas-compliance"
---

## Summary
The codebase implements raw ECDSA/EdDSA signing (`components/cdc_hal/src/Tropic01Element.cpp:479-530`) but does not produce qualified signature formats required by eIDAS 2.0:
- **CAdES** (CMS Advanced Electronic Signatures) for data objects
- **XAdES** (XML Advanced Electronic Signatures) for XML documents
- **PAdES** (PDF Advanced Electronic Signatures) for PDFs

**Evidence:**
- File: `components/cdc_hal/src/Tropic01Element.cpp:479-530` - `ecdsaSign()` and `eddsaSign()` return raw 64-byte signatures
- File: `components/mod_gpg/src/gpg.cpp:577-595` - `gpg_sign_hash()` returns raw signature bytes
- No CAdES/XAdES/PAdES wrapper structures

## Impact
For eIDAS 2.0 compliance:
1. **Qualified Electronic Signatures (QES)** require advanced signature formats with metadata
2. **Long-term validity (LTV)** requires embedding certificate chains and timestamps in signature
3. **Interoperability**: Raw signatures are not portable across systems
4. **Legal effect**: eIDAS Article 26 requires "advanced electronic signatures" which CAdES/XAdES/PAdES provide

Current raw signatures work for FIDO2/WebAuthn and GPG but do not meet eIDAS requirements for document signing.

## Evidence
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:479-508
SeResult Tropic01Element::ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen,
                                     uint8_t* sig, size_t* sigLen) {
    // ...
    // lt_ecc_ecdsa_sign: (handle, slot, msg, msg_len, rs_output_64bytes)
    lt_ret_t ret = lt_ecc_ecdsa_sign(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                      hash, static_cast<uint32_t>(hashLen), sig);
    if (ret == LT_OK) {
        *sigLen = TR01_ECDSA_EDDSA_SIGNATURE_LENGTH;  // Always 64 bytes (R,S)
    }
    // Returns raw R||S bytes, no signature wrapper
}
```

```cpp
// components/mod_gpg/src/gpg.cpp:577-595
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len) {
    // Returns raw signature from secure element
    if (s_metadata.curve == CDC_CURVE_P256) {
        return se_sign_p256(gpg_storage_sig_slot(), hash, hash_len, sig_out, sig_len);
    }
}
```

## Recommended Fix
1. Add CAdES-BES (Basic Electronic Signature) wrapper using MbedTLS CMS module
2. Create signature helper that wraps raw signature with:
   - Signature algorithm OID
   - Certificate chain (if available)
   - Optional timestamp from TSA
3. Add serial command: `SIGN_CADES <slot> <hash>` returning ASN.1 DER CAdES structure

**Scope:** Implement CAdES-BES wrapper for ECDSA signatures (one-hour task)

## References
- [eIDAS Regulation (EU) No 910/2014](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32014R0910) - Article 3 (Definitions: advanced electronic signature)
- [ETSI EN 319 142-1](https://www.etsi.org/deliver/etsi_en/319100_319199/31914201/01.01.01_60/en_31914201v010101p.pdf) - CAdES format
- [MbedTLS CMS](https://www.mbed-tls.com/documentation/3.5.0/cms_8h.html) - CMS (CAdES) module
