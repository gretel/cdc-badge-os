---
title: "[MEDIUM] Inconsistent parameter naming: camelCase abbreviations in ISecureElement.h"
severity: MEDIUM
domain: cdc_hal
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
In `components/cdc_hal/include/cdc_hal/ISecureElement.h`, parameter names use inconsistent camelCase abbreviations (`privKey`, `pubKey`, `hashLen`, `sigLen`, `msgLen`) while other parts of the codebase use different conventions.

**Evidence** (lines 73-95):
```cpp
// ISecureElement.h uses camelCase abbreviations
virtual SeResult eccImport(uint8_t slot, const uint8_t* privKey, EccCurve curve) = 0;
virtual SeResult eccGetPublicKey(uint8_t slot, uint8_t* pubKey, EccCurve* curve = nullptr) = 0;
virtual SeResult ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen,
                           uint8_t* sig, size_t* sigLen) = 0;
virtual SeResult eddsaSign(uint8_t slot, const uint8_t* msg, size_t msgLen,
                           uint8_t* sig) = 0;
```

## Impact
- **Readability**: Abbreviations like `privKey`, `sigLen`, `msgLen` are less readable than full names
- **Consistency**: Unlike the rest of the codebase which uses clearer names like `data`, `buffer`, `length`
- **Discoverability**: Developers searching for parameters may use different terms (e.g., `privateKey` vs `privKey`)

## Evidence
- File: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- Lines: 73-95
- Parameters: `privKey`, `pubKey`, `hashLen`, `sigLen`, `msgLen`
- Contrast with: `cdc_log.cpp`, `ServiceRegistry.cpp` which use clearer names like `name`, `data`, `buffer`

## Recommended Fix
Use clearer, consistent parameter names:
```cpp
// Change from:
virtual SeResult eccImport(uint8_t slot, const uint8_t* privKey, EccCurve curve) = 0;
virtual SeResult eccGetPublicKey(uint8_t slot, uint8_t* pubKey, EccCurve* curve = nullptr) = 0;
virtual SeResult ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen,
                           uint8_t* sig, size_t* sigLen) = 0;

// To:
virtual SeResult eccImport(uint8_t slot, const uint8_t* privateKey, EccCurve curve) = 0;
virtual SeResult eccGetPublicKey(uint8_t slot, uint8_t* publicKey, EccCurve* curve = nullptr) = 0;
virtual SeResult ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLength,
                           uint8_t* signature, size_t* signatureLength) = 0;
```

## References
- C++ Core Guidelines: [C.35: Use a consistent naming style](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C35)
- Google C++ Style Guide: [Variable names](https://google.github.io/styleguide/cppguide.html#Variable_Names)
