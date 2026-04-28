---
title: "[HIGH] Inconsistent function naming style: snake_case C-style vs C++ conventions"
severity: HIGH
domain: cdc_core
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
In `components/cdc_core/include/cdc_core/KeyFingerprint.h`, functions use snake_case C-style naming (`key_fingerprint_generate`, `key_fingerprint_from_pubkey`, `key_fingerprint_word`) while the rest of the codebase uses C++ style naming (PascalCase for classes, camelCase for methods).

**Evidence** (lines 14-20):
```c
// KeyFingerprint.h uses snake_case C-style
bool key_fingerprint_generate(uint8_t slot, char* buf, size_t len);
bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len);
const char* key_fingerprint_word(uint8_t index);

// While other files use C++ style:
// ServiceRegistry.cpp: registerService(), getService(), initAll()
// ModuleRegistry.h: getModule(), startModule(), stopAll()
```

## Impact
- **Consistency**: Breaks the established C++ naming convention in the codebase
- **Discoverability**: Developers looking for functions in the `cdc::core` namespace won't find these C-style functions easily
- **Maintainability**: Creates confusion about which naming convention to use for new code

## Evidence
- File: `components/cdc_core/include/cdc_core/KeyFingerprint.h`
- Lines: 14-20
- Functions: `key_fingerprint_generate`, `key_fingerprint_from_pubkey`, `key_fingerprint_word`
- Contrast with: `ServiceRegistry.h`, `ModuleRegistry.h`, `IModule.h` which use camelCase methods

## Recommended Fix
Rename functions to match C++ convention:
```cpp
// Change from:
bool key_fingerprint_generate(uint8_t slot, char* buf, size_t len);

// To:
bool generateFingerprint(uint8_t slot, char* buf, size_t len);
bool generateFingerprintFromPubkey(const uint8_t* pubkey, size_t pubkeyLen,
                                   char* buf, size_t len);
const char* getFingerprintWord(uint8_t index);
```

Place in a `cdc::core` namespace or as static functions in a class.

## References
- C++ Core Guidelines: [N.1: Use camelCase for functions and members](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#N1)
- Project convention: All other core components use camelCase (ServiceRegistry, ModuleRegistry, IModule)
