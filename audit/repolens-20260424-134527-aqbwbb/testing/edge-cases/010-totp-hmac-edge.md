---
title: "[MEDIUM] TOTP HMAC computation lacks edge case tests for algorithm boundaries"
severity: MEDIUM
domain: mod_totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `TotpStore::hmacCompute()` function at `TotpStore.cpp:433-465` handles HMAC computation for different algorithms but lacks edge case tests:

1. **Invalid algorithm value** - Algorithm = 5 (outside SHA1/SHA256/SHA512)
2. **Key length = 0** - Empty key
3. **Key length > SECRET_LEN** - Key larger than 32 bytes
4. **Data length = 0** - Empty data
5. **Null output buffer** - `output = nullptr`
6. **Small output buffer** - Buffer smaller than expected digest
7. **mbedtls_md_info_from_type returns nullptr** - Algorithm not supported

**Evidence** (file:line):
- `TotpStore.cpp:433-465` - `hmacCompute()` function
- `TotpStore.cpp:440-453` - Algorithm switch statement

```cpp
// hmacCompute at TotpStore.cpp:433-465
bool TotpStore::hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                            const uint8_t* data, size_t dataLen,
                            uint8_t* output, size_t* outputLen) const {
    mbedtls_md_type_t mdType;
    size_t expectedLen;

    switch (algo) {
        case TotpAlgorithm::SHA256:
            mdType = MBEDTLS_MD_SHA256;
            expectedLen = 32;
            break;
        case TotpAlgorithm::SHA512:
            mdType = MBEDTLS_MD_SHA512;
            expectedLen = 64;
            break;
        default:
            mdType = MBEDTLS_MD_SHA1;
            expectedLen = 20;
            break;
    }

    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(mdType);
    if (!mdInfo) {
        return false;
    }

    int ret = mbedtls_md_hmac(mdInfo, key, keyLen, data, dataLen, output);
    if (ret != 0) {
        return false;
    }

    if (outputLen) {
        *outputLen = expectedLen;
    }
    return true;
}
```

The `default` case returns SHA1 for any invalid algorithm, which might mask bugs.

## Impact
- **Wrong algorithm**: Invalid algorithm silently uses SHA1 instead of failing
- **Buffer overflow**: Small output buffer could overflow (mbedtls doesn't check size)
- **Null pointer dereference**: `mbedtls_md_info_from_type` could return nullptr

## Evidence
No tests exist for `hmacCompute()` edge cases. Used in `generate()` at line 410.

## Recommended Fix
Add edge case tests:

```cpp
void test_totp_hmac_invalid_algo() {
    TotpStore store;
    uint8_t key[20] = {0x42};
    uint8_t data[8] = {0x00};
    uint8_t output[64];
    
    // Test algorithm = 5 (invalid)
    bool result = store.hmacCompute((TotpAlgorithm)5, key, 20, data, 8, output, nullptr);
    // Currently returns SHA1, should test this behavior
    TEST_ASSERT_TRUE(result); // Falls back to SHA1
}

void test_totp_hmac_empty_key() {
    TotpStore store;
    uint8_t key[1] = {0};
    uint8_t data[8] = {0x00};
    uint8_t output[64];
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA1, key, 0, data, 8, output, nullptr);
    TEST_ASSERT_TRUE(result); // HMAC with empty key is valid
}

void test_totp_hmac_key_too_long() {
    TotpStore store;
    uint8_t key[100];
    memset(key, 0x42, 100);
    uint8_t data[8] = {0x00};
    uint8_t output[64];
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA1, key, 100, data, 8, output, nullptr);
    TEST_ASSERT_TRUE(result); // HMAC handles long keys
}

void test_totp_hmac_empty_data() {
    TotpStore store;
    uint8_t key[20] = {0x42};
    uint8_t data[1] = {0};
    uint8_t output[64];
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA1, key, 20, data, 0, output, nullptr);
    TEST_ASSERT_TRUE(result); // HMAC with empty data is valid
}

void test_totp_hmac_small_output() {
    TotpStore store;
    uint8_t key[20] = {0x42};
    uint8_t data[8] = {0x00};
    uint8_t output[10]; // Small buffer for SHA1 (20 bytes)
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA1, key, 20, data, 8, output, nullptr);
    // mbedtls might overflow small buffer - should test this!
    TEST_ASSERT_TRUE(result);
}

void test_totp_hmac_null_output_len() {
    TotpStore store;
    uint8_t key[20] = {0x42};
    uint8_t data[8] = {0x00};
    uint8_t output[64];
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA1, key, 20, data, 8, output, nullptr);
    TEST_ASSERT_TRUE(result);
    
    size_t len;
    result = store.hmacCompute(TotpAlgorithm::SHA1, key, 20, data, 8, output, &len);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(20, len);
}

void test_totp_hmac_sha256() {
    TotpStore store;
    uint8_t key[32];
    memset(key, 0x42, 32);
    uint8_t data[8] = {0x00};
    uint8_t output[64];
    size_t len;
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA256, key, 32, data, 8, output, &len);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(32, len);
}

void test_totp_hmac_sha512() {
    TotpStore store;
    uint8_t key[64];
    memset(key, 0x42, 64);
    uint8_t data[8] = {0x00};
    uint8_t output[64];
    size_t len;
    
    bool result = store.hmacCompute(TotpAlgorithm::SHA512, key, 64, data, 8, output, &len);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(64, len);
}
```

## References
- RFC 4234 HMAC specification
- mbedtls HMAC API documentation
- Cryptographic algorithm edge cases
