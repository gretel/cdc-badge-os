---
title: "[HIGH] Password vault stores passwords unencrypted (data-at-rest encryption missing)"
severity: HIGH
domain: encryption
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The password vault module (`mod_password`) stores passwords in plain text in TROPIC01 R-Memory slots. Unlike the GPG DEC key which uses AES-256-GCM encryption (see `GpgStorage.cpp`), the password vault stores the `password` field directly without any encryption. This violates NIS2 Article 20 which requires "encryption of relevant data" for data at rest.

## Impact
**NIS2 Art. 20 Encryption Gap**:
- Passwords are stored unencrypted in R-Memory
- If R-Memory is dumped (via JTAG, flash extraction, or TROPIC01 exploit), all passwords are exposed
- No protection against physical attacks or memory scraping
- Passwords are more sensitive than GPG keys, yet have weaker protection

## Evidence

1. **Password payload structure is plain text**:
   - File: `components/mod_password/src/PasswordStore.cpp:17-22`
   ```cpp
   struct PasswordPayload {
       char title[PasswordStore::TITLE_LEN];
       char username[PasswordStore::USERNAME_LEN];
       char password[PasswordStore::PASSWORD_LEN];  // Plain text!
       char url[PasswordStore::URL_LEN];
       uint8_t totpSlot;
       char notes[PasswordStore::NOTES_LEN];
   };
   ```

2. **Password written directly without encryption**:
   - File: `components/mod_password/src/PasswordStore.cpp:220-225`
   ```cpp
   PasswordPayload payload = {};
   copyText(payload.title, sizeof(payload.title), entry.title);
   copyText(payload.username, sizeof(payload.username), entry.username);
   copyText(payload.password, sizeof(payload.password), entry.password);  // Direct copy!
   copyText(payload.url, sizeof(payload.url), entry.url);
   ```

3. **Compare with GPG DEC key encryption**:
   - File: `components/mod_gpg/src/GpgStorage.cpp:255-300`
   ```cpp
   // Derive encryption key from PIN
   // ...
   ret = mbedtls_gcm_encrypt(
       &gcm_ctx,
       sizeof(uint32_t) + PRIVKEY_SIZE,  // 32-byte key + timestamp
       storage->salt,
       &storage.encrypted,  // Encrypted with AES-256-GCM
       &len
   );
   ```
   - GPG uses AES-256-GCM with salt and PIN-derived key
   - Password vault has no such protection

4. **No encryption configuration**:
   - File: `components/mod_password/include/mod_password/PasswordStore.h`
   - No encryption context, key derivation, or cipher state

## Recommended Fix

1. **Add AES-256-GCM encryption for passwords**:
   ```cpp
   // components/mod_password/include/mod_password/PasswordStore.h
   struct EncryptedPasswordPayload {
       uint8_t salt[16];           // Random salt for key derivation
       uint8_t iv[12];             // GCM initialization vector
       uint8_t encrypted[64 + 16]; // Encrypted password + 8-byte tag
       uint8_t tag[8];             // GCM authentication tag
   };
   ```

2. **Derive encryption key from Badge PIN**:
   - Use HKDF-SHA256 similar to GPG module
   - Key derivation: `HKDF(PIN, salt, info="password-vault")`
   - Store salt in R-Memory header

3. **Encrypt on write, decrypt on read**:
   ```cpp
   bool PasswordStore::addEntry(const PasswordEntry& entry) {
       // Derive key from PIN
       uint8_t key[32];
       derivePasswordKey(pin, &salt, key);
       
       // Encrypt password
       EncryptedPasswordPayload payload;
       mbedtls_gcm_encrypt(&gcm_ctx, entry.password, &payload.encrypted, &payload.tag);
       
       // Write to R-Memory
   }
   ```

4. **Add PIN verification requirement**:
   - Require PIN verification before reading passwords
   - Cache decrypted passwords in RAM with timeout
   - Clear cache on lock

5. **Update UI to show encryption status**:
   - Display "Encrypted" indicator in password list
   - Show PIN prompt when accessing passwords

## References
- [NIS2 Directive Art. 20 - Encryption](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [GPG Module Encryption Implementation](components/mod_gpg/src/GpgStorage.cpp)
- [MbedTLS GCM Documentation](https://tls.mbed.org/api/gcm_8h.html)

</content>