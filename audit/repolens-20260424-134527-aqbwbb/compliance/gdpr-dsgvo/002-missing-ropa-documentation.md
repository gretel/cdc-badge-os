---
title: "[MEDIUM] Missing ROPA (Record of Processing Activities) Documentation"
severity: MEDIUM
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The CDC Badge OS firmware processes personal data but lacks a comprehensive Record of Processing Activities (ROPA) as required by Article 30 of the GDPR/DSGVO. The ROPA should document all data processing operations including:
- Categories of personal data processed
- Purposes of processing
- Categories of data subjects
- Storage durations
- Recipients of data
- Technical and organizational measures

## Impact
**Legal Risk**: Art. 30 DSGVO requires controllers to maintain a ROPA. For a device that processes personal data, this is a fundamental compliance document. Without it, regulatory authorities cannot easily verify compliance during audits.

**Documentation Gap**: The code contains clear documentation of storage allocations (`tropic_slot_map.h`, module headers) but this information is not consolidated into a formal ROPA structure.

**Scalability**: As new modules are added (FIDO2, SSH keys, etc.), the lack of a ROPA template makes it harder to maintain compliance documentation.

## Evidence

### Personal Data Categories Found in Code

1. **Contact Data (vCard)** - `components/mod_vcard/src/vcard_store.cpp`
   - Names (FN, N fields)
   - Phone numbers (TEL)
   - Email addresses (EMAIL)
   - Addresses (ADR)
   - URLs (URL)
   - Organization (ORG)
   - Lines 206-256: Name parsing logic

2. **Authentication Data** - `components/cdc_core/src/PinManager.cpp`
   - PIN hashes (badge PIN, PW1, PW3)
   - Salt values for KDF
   - Iteration counts
   - Lines 1-663: Full PIN management

3. **Password Data** - `components/mod_password/src/PasswordStore.cpp`
   - Titles, usernames, passwords
   - URLs, notes, TOTP slot references
   - Lines 14-22: PasswordPayload struct

4. **TOTP Secrets** - `components/mod_totp/src/TotpStore.cpp`
   - Issuer names, account names
   - Base32 secrets (raw bytes)
   - Algorithm, period, digits settings
   - Lines 15-24: TotpPayload struct

5. **GPG Key Metadata** - `components/mod_gpg/src/GpgStorage.cpp`
   - ECC slot assignments
   - Encrypted DEC private key
   - Session keys
   - Lines 40-60: DecKeyStorage struct

6. **FIDO2 Credentials** - `components/mod_fido2/src/ctap2.cpp`
   - RP IDs, credential IDs
   - Counter values
   - PIN tokens
   - Lines 73-115: ClientPIN state

### Missing Documentation Elements

No file exists containing:
- Systematic list of all data categories
- Purposes for each data type
- Legal basis for processing (consent, contract, legitimate interest)
- Retention periods
- Data flow diagrams
- Third-party processors (if any)

## Recommended Fix

### Create ROPA Documentation File (~1 hour)
Add `docs/ROPA.md` with the following structure:

```markdown
# Record of Processing Activities (ROPA)
## CDC Badge OS - Firmware v1.0

### 1. Controller Information
- **Device**: CDC Badge v1.0/v1.1
- **Data Processing Location**: On-device (ESP32-S3 + TROPIC01)
- **Cross-border transfers**: None (all data stored locally)

### 2. Data Categories and Purposes

| Data Category | Purpose | Legal Basis | Retention |
|--------------|---------|-------------|-----------|
| vCard (own) | Contact display, BLE exchange | Consent (user input) | Until deleted |
| vCard (peers) | Contact storage, BLE exchange | Consent (user acceptance) | Until deleted |
| PIN hashes | Authentication | Contract (device access) | Until changed |
| Passwords | Credential storage | Contract (vault function) | Until deleted |
| TOTP secrets | Time-based authentication | Contract (TOTP function) | Until deleted |
| GPG keys | Encryption, signing | Contract (GPG function) | Until deleted |
| FIDO2 credentials | WebAuthn authentication | Contract (FIDO2 function) | Until deleted |

### 3. Technical and Organizational Measures
- **Encryption**: AES-256-GCM for DEC private key
- **Key Storage**: TROPIC01 secure element (ECC slots, R-Memory)
- **PIN Protection**: KDF (Iterated+Salted S2K), retry limits
- **Physical Security**: On-device only, no cloud sync
- **Data Minimization**: Only stores what user explicitly adds

### 4. Data Subjects
- Badge owner (own vCard, personal data)
- Contact persons (peer vCards)

### 5. Recipients
- No third-party processors
- All data stays on device

### 6. Retention Periods
- User-defined (data deleted when user requests)
- No automatic expiration (except lockout timers)
```

### Add Module Documentation Templates (~30 min)
Create `docs/MODULE_ROPA_TEMPLATE.md` for future modules:
```markdown
## Module: <name>

### Data Categories
- ...

### Purposes
- ...

### Storage Locations
- ...

### Retention
- ...
```

### Update CI/CD Checklist (~30 min)
Add ROPA update requirement to module development guide:
- Every new module must update ROPA.md
- ROPA.md must be reviewed before release

## References
- **Art. 30 DSGVO** - Record of processing activities
- **Article 29 Working Party Guidelines on ROPA** - https://ec.europa.eu/justice/data-protection/article-29/documentation/opinion-recommendation/files/2018/wp251_rev.01_en.pdf
- **BfD ROPA Template** - https://www.bfdi.bund.de/DE/Infothek/FragenAntworten/Verzeichnis-der-Beschraenkungen.html
