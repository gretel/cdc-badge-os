---
title: "[LOW] No EU Digital Identity Wallet (EUDIW) integration"
severity: LOW
domain: eidas-signatures
lens: eidas-2-0
labels:
  - "eudiw"
  - "digital-identity"
---

## Summary
The codebase implements FIDO2 and GPG functionality but does not integrate with the EU Digital Identity Wallet (EUDIW) standards. There's no support for:
- EUDIW presentation definitions
- PID (Personal Identification Data) attribute storage
- mDL (mobile Driving License) format
- ISO 18013-5 (mDL) or ISO/IEC 18013-5 (EUDIW) protocols

**Evidence:**
- File: `components/mod_fido2/` - FIDO2/WebAuthn only, no EUDIW
- File: `components/mod_gpg/` - OpenPGP only, no EUDIW
- File: `components/mod_totp/` - TOTP codes only, no EUDIW
- No search for "eudiw", "pid", "mdl", "iso18013" in codebase

## Impact
For eIDAS 2.0 compliance:
1. **EUDIW Regulation (EU) 2024/...** requires member states to support EUDIW by 2027
2. **Digital identity**: Badge should be able to act as EUDIW anchor or connector
3. **Interoperability**: EUDIW uses ISO 18013-5 and W3C Verifiable Credentials
4. **Future-proofing**: Without EUDIW support, badge may not be usable for eIDAS 2.0 services

Current implementation predates EUDIW final standards and focuses on FIDO2/GPG.

## Evidence
```bash
# No EUDIW-related code found
grep -r "eudiw\|pid\|mdl\|iso18013\|presentation" components/ --include="*.cpp" --include="*.h"
# Returns empty
```

```cpp
// components/mod_fido2/src/fido2.cpp - FIDO2 only
// No EUDIW presentation definitions, no PID attributes
```

## Recommended Fix
1. Add EUDIW module with:
   - PID (Personal Identification Data) storage structure
   - ISO 18013-5 mDL profile support
   - COSE-signed credential format
2. Add BLE/NFC transport for EUDIW (already has BLE, needs NFC or use BLE)
3. Document EUDIW integration path (standards are still evolving)

**Scope:** Add EUDIW module skeleton with PID storage structure (one-hour task, full implementation is larger)

## References
- [EUDIW Regulation (EU) 2024/...](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32024R...) - EU Digital Identity Wallet
- [ISO/IEC 18013-5](https://www.iso.org/standard/69084.html) - Mobile driving license
- [EUDIW Reference Framework](https://github.com/eu-digital-identity-wallet/eudi-doc-architecture-and-reference-framework) - GitHub repo
- [W3C Verifiable Credentials](https://www.w3.org/TR/vc-data-model/) - Data model
