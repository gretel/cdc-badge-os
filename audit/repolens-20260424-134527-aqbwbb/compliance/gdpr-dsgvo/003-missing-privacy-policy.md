---
title: "[LOW] Missing Privacy Policy / Data Protection Notice"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The CDC Badge OS firmware lacks a privacy policy or data protection notice that informs users about how their personal data is collected, processed, and stored. Article 13 and 14 of the GDPR/DSGVO require that data subjects be provided with specific information about data processing.

## Impact
**Legal Risk**: Users should be informed about data processing at the time of data collection. For a hardware device, this information should be available in user documentation or on the device itself.

**User Awareness**: Users may not understand what data is stored, where it's kept, and how long it's retained without a clear privacy notice.

**Transparency**: A privacy policy helps build trust and demonstrates compliance commitment.

## Evidence

### Existing Documentation
- `docs/README.md` - Technical overview, no privacy information
- `docs/UI_FLOWS.md` - User interface navigation
- `docs/SERIAL_COMMANDS.md` - Command reference
- `docs/MODULE_DEVELOPMENT.md` - Developer guide

### Missing Information
No document answers:
- What personal data is stored on the badge?
- Where is the data stored (TROPIC01, NVS, etc.)?
- Is data transmitted wirelessly (BLE, USB)?
- How long is data retained?
- How can users delete their data?
- What are the privacy settings?

### Data Processing Activities (from code analysis)
1. **vCard Storage** - `components/mod_vcard/src/vcard_store.cpp`
   - Stored in NVS (Non-Volatile Storage)
   - Up to 100 peer vCards + 1 own vCard
   - Lines 317-442: NVS read/write operations

2. **Password Vault** - `components/mod_password/src/PasswordStore.cpp`
   - Stored in TROPIC01 R-Memory
   - Encrypted storage
   - Lines 1-383: Full CRUD operations

3. **TOTP Secrets** - `components/mod_totp/src/TotpStore.cpp`
   - Stored in TROPIC01 R-Memory
   - Base32 encoded secrets
   - Lines 1-529: Account management

4. **PIN Management** - `components/cdc_core/src/PinManager.cpp`
   - Hashes stored in TROPIC01 R-Memory slot 0
   - KDF with salts and iteration counts
   - Lines 1-663: PIN verification and storage

## Recommended Fix

### Create Privacy Policy Document (~1 hour)
Add `docs/PRIVACY.md` with the following structure:

```markdown
# Privacy Policy - CDC Badge OS

## 1. Introduction
The CDC Badge stores personal data locally on the device. This document explains what data is stored, how it's used, and how you can manage it.

## 2. Data Stored on the Badge

### 2.1 Contact Data (vCards)
- **What**: Names, phone numbers, email addresses, addresses
- **Where**: NVS flash storage (up to 100 contacts + your own vCard)
- **Why**: Contact exchange via BLE, display on E-Paper screen
- **How long**: Until you delete them

### 2.2 Passwords
- **What**: Website titles, usernames, passwords, URLs, notes
- **Where**: TROPIC01 secure element R-Memory (slots 150-511)
- **Why**: Password vault for auto-typing credentials
- **How long**: Until you delete them

### 2.3 TOTP Secrets
- **What**: Account names, issuer names, secret keys
- **Where**: TROPIC01 secure element R-Memory (slots 32-131)
- **Why**: Generate time-based one-time passwords
- **How long**: Until you delete them

### 2.4 GPG Keys
- **What**: Public/private key pairs, fingerprints
- **Where**: TROPIC01 secure element ECC slots (1-31)
- **Why**: Encryption, signing, authentication
- **How long**: Until you delete them

### 2.5 FIDO2 Credentials
- **What**: Credential IDs, RP IDs, counter values
- **Where**: TROPIC01 secure element ECC slots (5-31)
- **Why**: WebAuthn/Passwordless authentication
- **How long**: Until you delete them

## 3. Data Transmission
- **BLE vCard Exchange**: Contacts sent to other badges when you initiate exchange
- **USB CDC/CCID**: Data transmitted to computer when connected
- **No cloud sync**: All data stays on the device

## 4. How to Manage Your Data
- **View data**: Use the badge menu to browse stored items
- **Delete single item**: Select item → Delete
- **Export all data**: Use serial command `DATA_EXPORT_ALL`
- **Delete all data**: Use serial command `DATA_DELETE_ALL` or factory reset

## 5. Security Measures
- **PIN protection**: Badge PIN (default 123456), OpenPGP PW1/PW3
- **Secure element**: TROPIC01 stores keys in hardware
- **Encryption**: AES-256-GCM for sensitive data
- **Lockout**: Automatic lock after failed PIN attempts

## 6. Contact
For privacy questions, contact the project maintainers at [GitHub issues](https://github.com/...).
```

### Add Privacy Summary to README (~30 min)
Add a brief privacy section to `docs/README.md`:
```markdown
## Privacy
See [PRIVACY.md](PRIVACY.md) for details on what data is stored and how to manage it.
```

### Add Serial Command for Privacy Info (~30 min)
Add `PRIVACY_INFO` command that outputs the privacy policy to serial:
```cpp
static void cmd_privacy_info(const char* args) {
    // Print privacy summary to serial
    Serial.println(F("CDC Badge Privacy Policy:"));
    Serial.println(F("1. vCards: Names, phones, emails (NVS storage)"));
    Serial.println(F("2. Passwords: Titles, users, passwords (TROPIC01)"));
    // ... etc
}
```

## References
- **Art. 13 DSGVO** - Information to be provided when personal data are collected
- **Art. 14 DSGVO** - Information to be provided when personal data have not been obtained from the data subject
- **BfD Privacy Policy Checklist** - https://www.bfdi.bund.de/
