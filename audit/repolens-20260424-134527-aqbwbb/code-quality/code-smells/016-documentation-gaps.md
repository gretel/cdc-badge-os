---
title: "[LOW] Comments: Some code could use better documentation"
severity: LOW
domain: mixed
lens: code-smells
labels:
  - "documentation"
  - "cdc_core"
---

## Summary
Some methods in the codebase lack sufficient documentation comments, making it harder for new developers to understand the purpose and usage. Specifically:
- `ModuleRegistry::buildSlotErrorMessage()` - private helper
- `ModuleRegistry::validateSlotRequest()` - if extracted
- `TotpStore::hmacCompute()` - algorithm details

## Impact
**Onboarding**: New developers need to read implementation to understand usage.

**Maintenance**: Hard to know what edge cases are handled.

**API discovery**: IDE tooltips don't show method purpose.

## Evidence
`components/cdc_core/src/ModuleRegistry.cpp:748-751`:
```cpp
void ModuleRegistry::buildSlotErrorMessage(char* buffer, size_t bufSize,
                                           const char* errorType, const char* mapName) {
    snprintf(buffer, bufSize, "%s for %s", errorType, mapName);
}
```

No documentation - what does this return? What are the buffer requirements?

`components/mod_totp/src/TotpStore.cpp:424-452`:
```cpp
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
        // ...
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

No documentation about output buffer requirements, algorithm support, etc.

## Recommended Fix
Add documentation to public and protected methods:

1. **buildSlotErrorMessage**:
```cpp
/**
 * \brief Builds standardized slot-validation error text.
 * \param buffer Output buffer for formatted message (must be at least bufSize).
 * \param bufSize Size of output buffer.
 * \param errorType Error category text (e.g., "missing ECC slot map").
 * \param mapName Slot map logical name.
 * \return void (writes to buffer).
 */
void buildSlotErrorMessage(char* buffer, size_t bufSize,
                           const char* errorType, const char* mapName);
```

2. **hmacCompute**:
```cpp
/**
 * \brief Computes HMAC for TOTP algorithm using mbedtls.
 * \param algo Hash algorithm (SHA1, SHA256, SHA512).
 * \param key HMAC key buffer.
 * \param keyLen Key length in bytes.
 * \param data Input data to hash.
 * \param dataLen Data length in bytes.
 * \param output Output digest buffer (must be at least 64 bytes for SHA512).
 * \param outputLen Optional output length pointer.
 * \return `true` on success, `false` if algorithm not supported or mbedtls error.
 * \note Output buffer must be large enough for the largest digest (SHA512 = 64 bytes).
 */
bool hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                 const uint8_t* data, size_t dataLen,
                 uint8_t* output, size_t* outputLen) const;
```

**Estimated effort**: ~1 hour to add documentation to key methods.

## References
- Doxygen manual: https://www.doxygen.nl/manual/commands.html
- Google C++ Style Guide: https://google.github.io/styleguide/cppguide.html#Function_Comments
