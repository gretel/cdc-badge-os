---
title: "[MEDIUM] TOTP code generation and time synchronization lacks integration tests"
severity: MEDIUM
domain: modules
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:mod_totp"
  - "area:totp"
---

## Summary
The `TotpStore` component (`components/mod_totp/src/TotpStore.cpp`) generates time-based one-time codes using HMAC-SHA1, but **no integration tests** verify correct code generation with known test vectors or time synchronization with system RTC.

## Impact
- **Code generation**: TOTP algorithm may produce incorrect codes
- **Time sync**: Codes may be off by one interval due to time sync issues
- **Base32 decoding**: Secret parsing may fail for edge cases
- **Algorithm/digits**: SHA256 and non-standard digit counts may be buggy

## Evidence

**TOTP generation** (`components/mod_totp/src/TotpStore.cpp:80-200`):
```cpp
uint32_t TotpStore::generateCode(uint8_t entryIndex) {
    // Get time from RTC
    time_t now = rtc->getEpoch();
    
    // Calculate time step (30s default)
    uint64_t t = now / period_;
    
    // Pack T into 8 bytes
    uint8_t data[8];
    data[0] = (t >> 56) & 0xff;
    // ...
    
    // HMAC-SHA1
    mbedtls_md_hmac(..., secret, secretLen, data, 8, hash);
    
    // Dynamic truncation
    int offset = hash[19] & 0x0f;
    uint32_t binary = ((hash[offset] & 0x7f) << 24) |
                      (hash[offset + 1] << 16) |
                      (hash[offset + 2] << 8) |
                      (hash[offset + 3]);
    
    // Modulo for digits
    uint32_t code = binary % powersOf10[digits_];
    return code;
}
```

**TOTP storage** (`components/mod_totp/src/TotpStore.cpp:200-350`):
```cpp
bool TotpStore::addEntry(const char* issuer, const char* secret,
                         uint8_t digits, uint32_t period) {
    // Decode Base32 secret
    uint8_t decoded[SECRET_LEN];
    int len = base32Decode(secret, decoded);
    
    // Store in R-Memory
    TotpPayload payload;
    // ...
    se->rmemWrite(slot, &payload, sizeof(TotpPayload));
}
```

**Base32 decoding** (`components/mod_totp/src/TotpStore.cpp:30-70`):
```cpp
static int base32Decode(const char* encoded, uint8_t* out, size_t outMax) {
    int buffer = 0;
    int bitsLeft = 0;
    // Standard Base32 (RFC 4648)
    for (const char* p = encoded; *p; ++p) {
        int value = base32CharValue(*p);
        // ...
    }
}
```

**Usage in TotpModule** (`components/mod_totp/src/TotpModule.cpp:200-500`):
```cpp
// Module uses storage for code generation
TotpStore::addEntry("Google", "JBSWY3DPEHPK3PXP", 6, 30);
uint32_t code = TotpStore::generateCode(0);
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_totp_generation/` that verifies:

1. **Known test vectors**: Generate codes matching RFC 6238 test vectors
2. **Base32 decoding**: Decode standard secrets correctly
3. **Storage round-trip**: Write and read entries correctly
4. **Time sync**: Codes sync with system time
5. **Algorithm variants**: SHA1, SHA256, SHA512 work
6. **Digit counts**: 6, 7, 8 digit codes work

**Test structure** (example):
```cpp
// test/test_totp_generation/test_totp_codes.cpp
#include "mod_totp/TotpStore.h"

void test_totp_rfc6238_test_vectors() {
    // RFC 6238 test vector: secret = "12345678901234567890" (ASCII)
    // Time = 59 (epoch seconds)
    // Expected: 287082 (SHA1, 6 digits)
    
    uint8_t secret[] = "12345678901234567890";
    uint32_t time = 59;
    
    uint32_t code = TotpStore::generateCodeForTime(
        secret, sizeof(secret) - 1,
        time, 30, 6
    );
    
    ASSERT_EQ(code, 287082);
}

void test_totp_base32_decode() {
    const char* secret = "JBSWY3DPEHPK3PXP";
    uint8_t decoded[20];
    
    int len = TotpStore::base32Decode(secret, decoded);
    
    ASSERT_EQ(len, 16);
    // Verify decoded bytes
}

void test_totp_storage_roundtrip() {
    // Add entry
    TotpStore::addEntry("Test", "JBSWY3DPEHPK3PXP", 6, 30);
    
    // Generate code
    uint32_t code = TotpStore::generateCode(0);
    
    // Verify code is 6 digits
    ASSERT_GE(code, 0);
    ASSERT_LT(code, 1000000);
}
```

## References
- [TotpStore implementation](components/mod_totp/src/TotpStore.cpp)
- [RFC 6238 TOTP spec](https://tools.ietf.org/html/rfc6238)
- [TOTP module](components/mod_totp/src/TotpModule.cpp)

</content>