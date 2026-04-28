---
title: "[MEDIUM] Missing const qualifiers on output pointer parameters"
severity: MEDIUM
domain: cdc_core
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
Several functions in `PinManager` and `KeyFingerprint` use output pointer parameters without `const` qualifiers, allowing accidental modification of the pointer itself. This affects:

- `PinManager::computeBadgeHash(const char* pin, uint8_t* hashOut)` - line 136 in PinManager.h
- `PinManager::computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut)` - line 139 in PinManager.h
- `key_fingerprint_generate(uint8_t slot, char* buf, size_t len)` - line 15 in KeyFingerprint.h
- `key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len, char* buf, size_t len)` - line 18 in KeyFingerprint.h

## Impact
Without `const` qualifiers on output pointers:
1. **Accidental reassignment**: The pointer itself can be reassigned within the function, potentially causing memory leaks or use-after-free if the original pointer was dynamically allocated.
2. **API clarity**: Callers cannot distinguish between input and output-only parameters.
3. **Compiler optimization**: The compiler may generate less optimal code since it cannot assume the pointer value remains constant.

## Evidence
In `components/cdc_core/include/cdc_core/PinManager.h`:
```cpp
bool computeBadgeHash(const char* pin, uint8_t* hashOut);      // Should be: uint8_t* const hashOut
bool computeKdfHash(const char* pin, const uint8_t* salt,      // salt is const, but hashOut is not
                    uint8_t* hashOut);
```

In `components/cdc_core/include/cdc_core/KeyFingerprint.h`:
```cpp
bool key_fingerprint_generate(uint8_t slot, char* buf, size_t len);  // Should be: char* const buf
bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len);  // Should be: char* const buf
```

The implementations in the `.cpp` files do not reassign these pointers, so adding `const` would be safe and improve type safety.

## Recommended Fix
Add `const` qualifiers to output pointer parameters to prevent accidental reassignment:

1. In `PinManager.h`:
   ```cpp
   bool computeBadgeHash(const char* pin, uint8_t* const hashOut);
   bool computeKdfHash(const char* pin, const uint8_t* const salt, uint8_t* const hashOut);
   ```

2. In `KeyFingerprint.h`:
   ```cpp
   bool key_fingerprint_generate(uint8_t slot, char* const buf, size_t len);
   bool key_fingerprint_from_pubkey(const uint8_t* const pubkey, size_t pubkey_len,
                                    char* const buf, size_t len);
   ```

3. Update corresponding function definitions in `.cpp` files to match.

## References
- C++ Core Guidelines F.46: "Use 'const' and 'constexpr' to help avoid unintended conversions and reassignments"
- C++ Core Guidelines C.43: "Declare a pointer parameter that is not reassigned as 'const'"

</content>