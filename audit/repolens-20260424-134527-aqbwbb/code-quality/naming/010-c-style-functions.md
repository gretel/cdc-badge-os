---
title: "[LOW] C-style function declarations in C++ header files"
severity: LOW
domain: cdc_core
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
In `components/cdc_core/include/cdc_core/KeyFingerprint.h`, C-style function declarations (with `extern "C"` and C types) are mixed with C++ conventions used elsewhere in the codebase.

**Evidence**:
```cpp
// KeyFingerprint.h uses C-style declarations
#ifndef __cplusplus
#define KEY_FINGERPRINT_WORD_COUNT 5
#define KEY_FINGERPRINT_MAX_LEN 64
#endif

// Generate alchemical fingerprint for ECC key slot (0-31)
bool key_fingerprint_generate(uint8_t slot, char* buf, size_t len);

// Generate fingerprint from raw public key
bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len);

// Lookup word for index (0-31)
const char* key_fingerprint_word(uint8_t index);

#ifdef __cplusplus
extern "C" {
#endif
```

While other headers use modern C++:
```cpp
// ServiceRegistry.h uses C++ style
namespace cdc::core {
class ServiceRegistry {
public:
    bool registerService(const char* name, IService* service);
    IService* getService(const char* name);
    template<typename T>
    T* get(const char* name);
};
}
```

## Impact
- **Consistency**: Breaks the C++ namespace and class conventions used throughout the codebase
- **Discoverability**: Functions aren't in the `cdc::core` namespace like other core functions
- **Maintainability**: Unclear whether to use C or C++ style for new utility functions

## Evidence
- File: `components/cdc_core/include/cdc_core/KeyFingerprint.h`
- Lines: 1-25
- C-style: `extern "C"`, `key_fingerprint_generate`, `key_fingerprint_from_pubkey`
- C++ style (contrast): `ServiceRegistry.h`, `ModuleRegistry.h`, `IModule.h`

## Recommended Fix
Convert to C++ style:
```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace cdc::core {

/**
 * \brief Generate alchemical fingerprint for ECC key slot.
 * \param slot ECC slot number (0-31).
 * \param buf Output buffer.
 * \param len Buffer size.
 * \return true if fingerprint generated.
 */
bool generateFingerprint(uint8_t slot, char* buf, size_t len);

/**
 * \brief Generate fingerprint from raw public key.
 * \param pubkey Public key bytes.
 * \param pubkeyLen Public key length.
 * \param buf Output buffer.
 * \param len Buffer size.
 * \return true if fingerprint generated.
 */
bool generateFingerprintFromPubkey(const uint8_t* pubkey, size_t pubkeyLen,
                                   char* buf, size_t len);

/**
 * \brief Lookup word for index.
 * \param index Word index (0-31).
 * \return Constant string word.
 */
const char* getFingerprintWord(uint8_t index);

} // namespace cdc::core
```

## References
- C++ Core Guidelines: [F.1: Use `extern "C"` only to prevent C++ name-mangling](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#F1)
- C++ Core Guidelines: [M.1: Use namespaces](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#M1)
