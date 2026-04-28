---
title: "[LOW] Hardcoded TOTP Algorithm Selection"
severity: LOW
domain: architecture/extensibility
lens: crypto-algorithms
labels:
  - "audit:architecture/extensibility"
---

## Summary
TOTP algorithm selection (SHA1, SHA256, SHA512) is implemented as a hardcoded switch statement in `components/mod_totp/src/TotpStore.cpp`. Adding new algorithms (e.g., SHA384, SHA512/256) requires modifying the store code.

**Files affected:**
- `components/mod_totp/src/TotpStore.cpp` (lines 443-456)
- `components/mod_totp/include/mod_totp/TotpStore.h` (lines 9-12)

## Impact
- **Algorithm extensibility**: Adding new TOTP algorithms requires modifying core store logic
- **Crypto agility**: Cannot easily swap to new algorithms as standards evolve
- **Code duplication**: Each algorithm case has similar logic that could be factored out

## Evidence

### Hardcoded Algorithm Switch
`components/mod_totp/src/TotpStore.cpp:443-456`:
```cpp
bool TotpStore::hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                            const uint8_t* data, size_t dataLen,
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
```

### Fixed Algorithm Enum
`components/mod_totp/include/mod_totp/TotpStore.h:9-12`:
```cpp
enum class TotpAlgorithm : uint8_t {
    SHA1 = 0,
    SHA256 = 1,
    SHA512 = 2
};
```

## Recommended Fix

### Create Algorithm Strategy Interface
```cpp
// components/mod_totp/include/mod_totp/AlgorithmStrategy.h
#pragma once
#include <cstdint>
#include <cstddef>

namespace cdc::mod_totp {

class IHmacStrategy {
public:
    virtual ~IHmacStrategy() = default;
    virtual const char* getName() const = 0;
    virtual uint8_t getDigestSize() const = 0;
    virtual bool compute(const uint8_t* key, size_t keyLen,
                         const uint8_t* data, size_t dataLen,
                         uint8_t* output, size_t* outputLen) const = 0;
};

class TotpAlgorithmRegistry {
public:
    static TotpAlgorithmRegistry& instance();
    void registerStrategy(uint8_t id, IHmacStrategy* strategy);
    IHmacStrategy* getStrategy(uint8_t id);
    IHmacStrategy* getStrategyByName(const char* name);
};

// Built-in strategies
class Sha1Strategy : public IHmacStrategy {
    const char* getName() const override { return "SHA1"; }
    uint8_t getDigestSize() const override { return 20; }
    bool compute(const uint8_t* key, size_t keyLen,
                 const uint8_t* data, size_t dataLen,
                 uint8_t* output, size_t* outputLen) const override;
};

class Sha256Strategy : public IHmacStrategy {
    const char* getName() const override { return "SHA256"; }
    uint8_t getDigestSize() const override { return 32; }
    bool compute(const uint8_t* key, size_t keyLen,
                 const uint8_t* data, size_t dataLen,
                 uint8_t* output, size_t* outputLen) const override;
};

class Sha512Strategy : public IHmacStrategy {
    const char* getName() const override { return "SHA512"; }
    uint8_t getDigestSize() const override { return 64; }
    bool compute(const uint8_t* key, size_t keyLen,
                 const uint8_t* data, size_t dataLen,
                 uint8_t* output, size_t* outputLen) const override;
};

} // namespace cdc::mod_totp
```

### Refactor to Use Registry
```cpp
// components/mod_totp/src/TotpStore.cpp
bool TotpStore::hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                            const uint8_t* data, size_t dataLen,
                            uint8_t* output, size_t* outputLen) const {
    auto& registry = AlgorithmRegistry::instance();
    auto* strategy = registry.getStrategy(static_cast<uint8_t>(algo));
    if (!strategy) {
        return false;
    }
    return strategy->compute(key, keyLen, data, dataLen, output, outputLen);
}
```

### Add New Algorithms Without Modifying Core
```cpp
// components/mod_totp_sha384/src/Sha384Strategy.cpp
class Sha384Strategy : public IHmacStrategy {
    const char* getName() const override { return "SHA384"; }
    uint8_t getDigestSize() const override { return 48; }
    bool compute(const uint8_t* key, size_t keyLen,
                 const uint8_t* data, size_t dataLen,
                 uint8_t* output, size_t* outputLen) const override {
        mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
        return mbedtls_md_hmac(mdInfo, key, keyLen, data, dataLen, output) == 0;
    }
};

void mod_totp_sha384_register() {
    auto& registry = AlgorithmRegistry::instance();
    registry.registerStrategy(3, new Sha384Strategy());
}
```

## References
- Strategy Pattern: https://refactoring.guru/design-patterns/strategy
- Crypto agility: https://csrc.nist.gov/glossary/term/crypto_agility
- RFC 4226 (HOTP): https://datatracker.ietf.org/doc/html/rfc4226
- RFC 6238 (TOTP): https://datatracker.ietf.org/doc/html/rfc6238

</content>