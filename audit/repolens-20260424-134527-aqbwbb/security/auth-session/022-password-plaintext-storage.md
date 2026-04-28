---
title: "[HIGH] Password vault stores passwords in plaintext"
severity: HIGH
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The password vault module stores passwords in **plaintext** within TROPIC01 R-Memory slots with no encryption or hashing. Unlike TOTP secrets (which are at least protected by hardware), passwords are stored in a simple plaintext structure that can be read directly from storage.

**File**: `components/mod_password/include/mod_password/PasswordStore.h:21-28`
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

**File**: `components/mod_password/src/PasswordStore.cpp:16-20`
```cpp
#pragma pack(push, 1)
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];
    char username[PasswordStore::USERNAME_LEN];
    char password[PasswordStore::PASSWORD_LEN];  // Plaintext storage
    char url[PasswordStore::URL_LEN];
    uint8_t totpSlot;
    char notes[PasswordStore::NOTES_LEN];
};
#pragma pack(pop)
```

**File**: `components/mod_password/src/PasswordStore.cpp:217-240`
```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // ...
    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    copyText(payload.username, sizeof(payload.username), entry.username);
    copyText(payload.password, sizeof(payload.password), entry.password);  // Direct copy
    copyText(payload.url, sizeof(payload.url), entry.url);
    // ...
    
    auto res = se->rmemWriteWithHeader(
        slot,
        moduleId_,
        headerName,
        0,
        reinterpret_cast<const uint8_t*>(&payload),
        sizeof(payload)  // Written as-is, no encryption
    );
}
```

The password is read back and returned in plaintext:
**File**: `components/mod_password/src/PasswordStore.cpp:147`
```cpp
copyText(out->password, sizeof(out->password), payload.password);  // Plaintext read
```

## Impact
- **Physical Access Attack**: An attacker with access to the TROPIC01 chip can extract all passwords directly from R-Memory
- **No Key Derivation**: Passwords are not derived from a master key, so each password is independent and directly readable
- **Password Reuse Risk**: Since passwords are stored plaintext, users may be more likely to reuse them
- **Data Breach Impact**: If the device is compromised, all stored passwords are immediately usable
- **Comparison to TOTP**: TOTP secrets at least use HMAC for code generation, but passwords are completely plaintext

## Evidence
**File**: `components/mod_password/include/mod_password/PasswordStore.h:24`
```cpp
char password[PASSWORD_PASSWORD_LEN + 1];  // No encryption, no hashing
```

**File**: `components/mod_password/src/PasswordStore.cpp:18-19`
```cpp
char password[PasswordStore::PASSWORD_LEN];  // Plaintext in payload
```

**File**: `components/mod_password/src/PasswordStore.cpp:217-240`
Password is written directly to R-Memory without any transformation.

**File**: `components/mod_password/src/PasswordStore.cpp:147`
Password is read back and returned in plaintext to callers.

**No encryption found**:
```
grep -n "encrypt\|decrypt\|AES\|GCM\|CBC" components/mod_password/src/PasswordStore.cpp
# No results - no encryption at all!
```

## Recommended Fix
Implement encryption for password storage using a master key:

1. **Generate Master Key**: Store a master key in a dedicated TROPIC01 ECC slot (e.g., slot 0)
2. **Encrypt on Write**: Encrypt passwords before storing in R-Memory
3. **Decrypt on Read**: Decrypt passwords when reading from storage
4. **Use AES-GCM**: Use authenticated encryption for integrity protection

Example implementation:
```cpp
#include <mbedtls/gcm.h>

class PasswordStore {
private:
    static constexpr int MASTER_SLOT = 0;  // ECC slot for master key
    
    bool getMasterKey(uint8_t key[32]) {
        auto* se = cdc::hal::getSecureElementInstance();
        return se->eccRead(MASTER_SLOT, key, 32) == cdc::hal::SeResult::OK;
    }
    
    bool encryptPassword(const char* plain, size_t len, uint8_t* out, size_t* outLen) {
        mbedtls_gcm_context gcm;
        uint8_t iv[12];
        uint8_t masterKey[32];
        
        esp_fill_random(iv, 12);  // Random IV
        getMasterKey(masterKey);
        
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, masterKey, 256);
        
        int ret = mbedtls_gcm_crypt_and_tag(
            &gcm, MBEDTLS_GCM_ENCRYPT, len, iv, 12,
            (const uint8_t*)plain, out, 16, out + len  // Tag
        );
        
        mbedtls_gcm_free(&gcm);
        *outLen = len + 16;  // Ciphertext + tag
        return ret == 0;
    }
    
    bool decryptPassword(const uint8_t* cipher, size_t len, char* out, size_t* outLen) {
        mbedtls_gcm_context gcm;
        uint8_t iv[12];
        uint8_t masterKey[32];
        
        esp_fill_random(iv, 12);  // IV stored with ciphertext
        getMasterKey(masterKey);
        
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, masterKey, 256);
        
        int ret = mbedtls_gcm_crypt_and_tag(
            &gcm, MBEDTLS_GCM_DECRYPT, len, iv, 12,
            cipher, (uint8_t*)out, 16, cipher + len  // Tag
        );
        
        mbedtls_gcm_free(&gcm);
        *outLen = len;
        return ret == 0;
    }
};
```

Alternatively, use the TROPIC01 for symmetric encryption if available.

## References
- OWASP Password Storage Cheat Sheet
- NIST SP 800-63B - Authentication and Secure Storage
- RFC 4086 - Randomness Requirements for Security
- TROPIC01 Datasheet - ECC vs R-Memory Storage

</content>