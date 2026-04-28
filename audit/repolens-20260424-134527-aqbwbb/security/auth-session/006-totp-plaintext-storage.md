---
title: "[LOW] TOTP secrets stored without encryption"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
TOTP secrets are stored in TROPIC01 R-Memory slots without encryption. While the secure element provides hardware protection, the secrets are stored in plaintext within the R-Memory structure:

**File**: `components/mod_totp/src/TotpStore.cpp:16-23`
```cpp
#pragma pack(push, 1)
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];
    uint8_t secret[TotpStore::SECRET_LEN];  // Plaintext secret
    uint8_t secretLen;
    uint8_t digits;
    uint8_t period;
    uint8_t algorithm;
    uint8_t flags;
};
#pragma pack(pop)
```

**File**: `components/mod_totp/src/TotpStore.cpp:270-288`
```cpp
bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32,
                           uint8_t digits, uint32_t period, uint8_t algorithm) {
    // ... decode Base32 secret ...
    
    TotpPayload payload = {};
    if (issuer) {
        strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
    }
    memcpy(payload.secret, secret, static_cast<size_t>(secretLen));  // Plaintext write
    payload.secretLen = static_cast<uint8_t>(secretLen);
    // ...
    
    auto res = se->rmemWriteWithHeader(
        slot,
        moduleId_,
        name,
        0,
        reinterpret_cast<const uint8_t*>(&payload),
        sizeof(payload)
    );
}
```

## Impact
- **Physical Access Attack**: An attacker with access to the TROPIC01 chip could read R-Memory slots directly
- **TOTP Compromise**: If secrets are extracted, all TOTP codes can be generated offline
- **No Key Derivation**: Secrets are not derived from a master key, so each secret is independent

## Evidence
**File**: `components/mod_totp/src/TotpStore.cpp:16-23`
```cpp
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];
    uint8_t secret[TotpStore::SECRET_LEN];  // Plaintext, not encrypted
    uint8_t secretLen;
    ...
};
```

**File**: `components/mod_totp/src/TotpStore.cpp:270-288`
Secret is written directly to R-Memory without encryption.

## Recommended Fix
1. **Encrypt secrets**: Use a master key (stored in TROPIC01 ECC slot) to encrypt TOTP secrets before storage
2. **Use ECC slots**: Store TOTP secrets in ECC slots instead of R-Memory for better hardware protection
3. **Add key derivation**: Derive per-account secrets from a master secret using HKDF

Example implementation:
```cpp
// Add master key encryption
bool TotpStore::encryptSecret(const uint8_t* secret, uint8_t len, uint8_t* out) {
    // Use TROPIC01 ECC slot 0 as master key
    auto* se = cdc::hal::getSecureElementInstance();
    uint8_t iv[16] = {};
    esp_fill_random(iv, 16);  // Random IV
    
    // Get master key from ECC slot
    uint8_t masterKey[32];
    se->eccRead(0, masterKey, 32);  // Read master key
    
    // Encrypt with AES-GCM
    return aes_gcm_encrypt(masterKey, 32, iv, 16, secret, len, out);
}
```

## References
- RFC 6238 - TOTP: Time-Based One-Time Password Algorithm
- TROPIC01 Datasheet - R-Memory vs ECC Storage
- NIST SP 800-63B - Authentication Factors
