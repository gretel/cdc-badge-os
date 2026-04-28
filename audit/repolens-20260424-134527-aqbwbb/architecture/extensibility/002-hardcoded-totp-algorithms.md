---
title: "[LOW] TOTP algorithm selection uses switch statement instead of strategy pattern"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
In `components/mod_totp/src/TotpStore.cpp:438-453`, the HMAC algorithm selection for TOTP generation uses a hardcoded switch statement. Adding a new algorithm (e.g., SHA384, BLAKE2) requires modifying this central function.

**Evidence:**
- `components/mod_totp/src/TotpStore.cpp:438-453` - Switch statement for algorithm selection

## Impact
- **Maintenance**: Every new algorithm requires editing the core TOTP logic
- **Scalability**: Switch statement grows with each new algorithm
- **Testability**: Hard to inject mock algorithms for testing

## Evidence
File: `components/mod_totp/src/TotpStore.cpp:438-453`
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
        case TotpAlgorithm::SHA512:
            mdType = MBEDTLS_MD_SHA512;
            expectedLen = 64;
            break;
        default:
            mdType = MBEDTLS_MD_SHA1;
            expectedLen = 20;
            break;
    }
    // ...
}
```

## Recommended Fix
Implement a strategy pattern for HMAC computation:

1. **Define algorithm interface** in `components/mod_totp/include/mod_totp/HmacStrategy.h`:
   ```cpp
   class IHmacStrategy {
   public:
       virtual ~IHmacStrategy() = default;
       virtual mbedtls_md_type_t getType() const = 0;
       virtual size_t getDigestLength() const = 0;
       virtual bool compute(const uint8_t* key, size_t keyLen,
                           const uint8_t* data, size_t dataLen,
                           uint8_t* output) const = 0;
   };
   ```

2. **Create implementations** for each algorithm:
   ```cpp
   class HmacSha1Strategy : public IHmacStrategy { /* ... */ };
   class HmacSha256Strategy : public IHmacStrategy { /* ... */ };
   class HmacSha512Strategy : public IHmacStrategy { /* ... */ };
   ```

3. **Use registry in TotpStore**:
   ```cpp
   class HmacRegistry {
   public:
       static HmacRegistry& instance();
       void registerStrategy(TotpAlgorithm algo, IHmacStrategy* strategy);
       IHmacStrategy* getStrategy(TotpAlgorithm algo);
   };
   ```

4. **Refactor `hmacCompute()`** to delegate:
   ```cpp
   bool TotpStore::hmacCompute(TotpAlgorithm algo, ...) const {
       auto* strategy = HmacRegistry::instance().getStrategy(algo);
       if (!strategy) return false;
       return strategy->compute(key, keyLen, data, dataLen, output);
   }
   ```

Now adding a new algorithm only requires creating a new strategy class and registering it - no modification to `TotpStore`.

## References
- Strategy Pattern: Encapsulate interchangeable algorithms
- Open/Closed Principle: Extend with new algorithms without modifying existing code
- Dependency Injection: Inject algorithm implementations for testability
