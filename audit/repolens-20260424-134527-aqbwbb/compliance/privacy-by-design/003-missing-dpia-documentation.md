---
title: "[LOW] No Privacy Impact Assessment (DPIA) documentation for high-risk processing"
severity: LOW
domain: Privacy by Design
lens: Missing-Privacy-Impact-Assessment
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The repository lacks a Privacy Impact Assessment (PIA) or Data Protection Impact Assessment (DPIA) document for modules that process personal data. The following modules handle high-risk data that should have documented privacy assessments:

- **mod_password**: Stores passwords, usernames, URLs (potentially sensitive)
- **mod_totp**: Stores TOTP secrets for 2FA (cryptographic secrets)
- **mod_vcard**: Stores contact information (names, emails, phones, social profiles)
- **mod_gpg**: Stores GPG key metadata and encrypted private keys

## Impact
- **Regulatory Non-Compliance:** GDPR Article 35 requires DPIAs for processing that involves "systematic and extensive processing of personal data" or "large scale processing of special categories of data."
- **Unidentified Privacy Risks:** Without formal assessment, privacy risks may be overlooked during feature development.
- **No Privacy Review Process:** Developers lack guidance on when to conduct privacy reviews for new features.
- **Audit Readiness:** During compliance audits, lack of DPIA documentation may result in findings or require retrospective assessment.

## Evidence
**Documentation check:**
```
$ find docs/ -name "*privacy*" -o -name "*DPIA*" -o -name "*impact*"
docs/
(no results)
```

**Modules processing personal data without DPIA:**
- `components/mod_password/` - Password vault (32 passwords, usernames, URLs)
- `components/mod_totp/` - TOTP secrets (100 accounts)
- `components/mod_vcard/` - Contact data (100 vCards with names, emails, phones)
- `components/mod_gpg/` - GPG key metadata
- `components/mod_fido2/` - FIDO2 credentials with RP IDs and user names

**File:** `docs/README.md` - No reference to privacy assessment process.

## Recommended Fix
1. **Create a DPIA template** in `docs/privacy/DPIA-template.md`:
   - Description of processing operations
   - Necessity and proportionality assessment
   - Risk assessment (to rights and freedoms)
   - Measures to address risks

2. **Create DPIA documents for existing modules:**
   - `docs/privacy/DPIA-password-module.md`
   - `docs/privacy/DPIA-totp-module.md`
   - `docs/privacy/DPIA-vcard-module.md`
   - `docs/privacy/DPIA-gpg-module.md`

3. **Add privacy review checklist** to `docs/MODULE_DEVELOPMENT.md`:
   - [ ] Privacy Impact Assessment completed?
   - [ ] Data classification applied?
   - [ ] PII logging reviewed?
   - [ ] Access controls documented?

4. **Create a privacy guide** `docs/privacy/PRIVACY_GUIDE.md` with:
   - Data classification definitions
   - When to conduct a DPIA
   - PII handling guidelines
   - Logging best practices

## References
- GDPR Article 35 - Data Protection Impact Assessment
- GDPR Article 36 - Prior consultation
- ISO/IEC 29134 - DPIA guidelines
- AICPA Privacy Maturity Model
