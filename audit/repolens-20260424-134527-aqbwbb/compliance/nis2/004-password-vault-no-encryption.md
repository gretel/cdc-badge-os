---
title: "[MEDIUM] Password vault stores passwords unencrypted in secure element"
severity: MEDIUM
domain: encryption
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The password vault module (`mod_password`) stores passwords directly in TROPIC01 R-Memory slots without additional encryption. While the TROPIC01 provides tamper-resistant storage, NIS2 requires "encryption of relevant digital information" (Art. 20). The passwords are stored as plaintext within the secure element's R-Memory, relying solely on the secure element's physical protection.

## Impact
**NIS2 Art. 20 Encryption Gap**: 
- If TROPIC01 is physically compromised, passwords are exposed
- No application-level encryption for defense-in-depth
- Password data not encrypted at rest beyond secure element
- No key management for application-level encryption

## Evidence
1. **Password structure is plaintext**:
   - File: `components/mod_password/include/mod_password/PasswordStore.h:21-28`
   ```cpp
   struct PasswordEntry {
       char title[PASSWORD_TITLE_LEN + 1];
       char username[PASSWORD_USERNAME_LEN + 1];
       char password[PASSWORD_PASSWORD_LEN + 1];  // Plaintext!
       char url[PASSWORD_URL_LEN + 1];
       uint8_t totpSlot;
       char notes[PASSWORD_NOTES_LEN + 1];
   };
   ```

2. **No encryption implementation**:
   - File: `components/mod_password/src/PasswordStore.cpp`
   - `addEntry()`, `updateEntry()`, `readEntry()` - No encrypt/decrypt calls
   - Passwords written directly to R-Memory:
   ```cpp
   copyText(payload.password, sizeof(payload.password), entry.password);
   auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0,
                                      reinterpret_cast<const uint8_t*>(&payload),
                                      sizeof(payload));
   ```

3. **No encryption key management**:
   - No key derivation from user PIN
   - No AES encryption for password data
   - No key storage separate from data

## Recommended Fix
1. **Add AES-256-GCM Encryption**:
   - Derive encryption key from user PIN using PBKDF2
   - Encrypt password payload before writing to R-Memory
   - Use TROPIC01 ECC slot for key wrapping if needed

2. **Implement Key Management**:
   - Store encryption key in TROPIC01 ECC slot
   - Wrap key with PIN-derived key
   - Implement key rotation procedure

3. **Modify PasswordStore**:
   - Add `encryptPayload()` and `decryptPayload()` methods
   - Update `addEntry()` to encrypt before write
   - Update `readEntry()` to decrypt after read
   - Add integrity check (GCM tag)

4. **Document Encryption Scheme**:
   - Document key derivation function
   - Document cipher mode and key size
   - Document key storage location

## References
- [NIS2 Directive Art. 20 - Encryption](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-132 - Password-Based Key Derivation](https://csrc.nist.gov/publications/detail/sp/800-132/final)
- [NIST SP 800-38D - GCM Mode](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
