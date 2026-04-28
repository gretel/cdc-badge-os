---
title: "[MEDIUM] Hardcoded GPG fingerprint sizes (20 bytes for SHA-1)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The OpenPGP implementation uses hardcoded value `20` for fingerprint sizes throughout `components/mod_gpg/src/openpgp/openpgp.cpp`. This represents SHA-1 hash length (used in OpenPGP fingerprint format) but is repeated as a magic number in multiple places without a named constant.

**Locations:** Lines 937, 946, 954, 963, 971, 979

## Impact
- **Maintainability**: If OpenPGP spec changes or different hash sizes are needed, all occurrences must be found and updated.
- **Clarity**: The value `20` doesn't explain it's SHA-1 fingerprint length.
- **Consistency**: 6+ occurrences scattered across similar code patterns.

## Evidence

**Lines 937-979 (fingerprint storage):**
```cpp
// Line 937: SIG fingerprint
case DO_FP_SIG:
    if (apdu->lc == 20) {
        memcpy(fingerprint_sig, apdu->data, 20);
        ...
    }

// Line 946: DEC fingerprint
case DO_FP_DEC:
    if (apdu->lc == 20) {
        memcpy(fingerprint_dec, apdu->data, 20);
        ...
    }

// Line 954: AUT fingerprint
case DO_FP_AUT:
    if (apdu->lc == 20) {
        memcpy(fingerprint_aut, apdu->data, 20);
        ...
    }

// Lines 963-979: CA fingerprints (3 more occurrences)
case DO_CA_FP_1:
    if (apdu->lc == 20) {
        memcpy(ca_fp_1, apdu->data, 20);
        ...
    }
```

Each fingerprint DO (Data Object) checks for length 20 and copies 20 bytes:
- `DO_FP_SIG` (signature key fingerprint)
- `DO_FP_DEC` (decryption key fingerprint)
- `DO_FP_AUT` (authentication key fingerprint)
- `DO_CA_FP_1`, `DO_CA_FP_2`, `DO_CA_FP_3` (CA fingerprints)

## Recommended Fix

1. **Define a named constant** near other OpenPGP constants at the top of the file:
```cpp
/** \brief OpenPGP fingerprint size (SHA-1 hash in bytes) */
static constexpr size_t OPENPGP_FINGERPRINT_SIZE = 20;
```

2. **Replace all occurrences** of `20` with the constant:
```cpp
// Before:
case DO_FP_SIG:
    if (apdu->lc == 20) {
        memcpy(fingerprint_sig, apdu->data, 20);
        ...
    }

// After:
case DO_FP_SIG:
    if (apdu->lc == OPENPGP_FINGERPRINT_SIZE) {
        memcpy(fingerprint_sig, apdu->data, OPENPGP_FINGERPRINT_SIZE);
        ...
    }
```

3. **Add context** in the constant's documentation explaining this is the OpenPGP v3 fingerprint format (SHA-1 of key material).

## References
- [OpenPGP Smart Card Application 3.4.1](https://github.com/ANSSI-FR/openpgp-card)
- [FIDO2 Signature Sizes](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html)
- Existing finding: `016-hardcoded-fido2-signature-sizes.md`
