---
title: "[MEDIUM] Hardcoded FNV-1a hash constants scattered across modules"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The FNV-1a hash algorithm constants (offset basis and FNV prime) are hardcoded as magic numbers in three different files without centralized definitions. These well-known constants should be defined once and reused for consistency and clarity.

**Files affected:**
- `components/cdc_core/src/TropicSlotMap.cpp:208` - `uint32_t hash = 2166136261u;`
- `components/cdc_core/src/TropicSlotMap.cpp:211` - `hash *= 16777619u;`
- `components/cdc_core/src/TropicStorage.cpp:418` - `uint32_t hash = 2166136261u;`
- `components/cdc_core/src/TropicStorage.cpp:421` - `hash *= 16777619u;`
- `components/mod_vcard/src/vcard_store.cpp:46` - `uint32_t hash = 2166136261u;`
- `components/mod_vcard/src/vcard_store.cpp:49` - `hash *= 16777619u;`

**Magic values:**
- `2166136261u` - FNV-1a 32-bit offset basis
- `16777619u` - FNV-1a 32-bit prime

## Impact
- **Maintainability**: If a different hash algorithm is needed, changes must be made in multiple locations.
- **Clarity**: The magic numbers don't convey "this is FNV-1a hash" without reading comments or context.
- **Consistency**: Each implementation repeats the same constants, increasing the risk of typos or variations.
- **Discoverability**: Developers searching for hash utilities must find all three locations.

## Evidence

**TropicSlotMap.cpp:208-211:**
```cpp
uint32_t computeMapSignature() const {
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };
    // ...
}
```

**TropicStorage.cpp:418-421:**
```cpp
uint32_t computeMapSignature() const {
    // FNV-1a 32-bit over map constants
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };
    // ...
}
```

**vcard_store.cpp:45-51:**
```cpp
static uint32_t fnv1a_hash(const char* data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 16777619u;
    }
    return hash;
}
```

## Recommended Fix

1. **Create a centralized hash constants header** at `components/cdc_core/include/cdc_core/hash_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief FNV-1a hash constants (32-bit)
     * 
     * Based on:
     * - FNV-1a 32-bit offset basis: 2166136261 (0x811C9DC5)
     * - FNV-1a 32-bit prime: 16777619 (0x01000193)
     * 
     * Reference: https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-1a
     */
    namespace cdc::core {
    namespace hash {
        static constexpr uint32_t FNV1A_OFFSET_BASIS_32 = 2166136261u;
        static constexpr uint32_t FNV1A_PRIME_32 = 16777619u;
    }
    }
    ```

2. **Create a helper function** for common hash operations:
    ```cpp
    /**
     * \brief Computes FNV-1a 32-bit hash.
     * \param data Input buffer.
     * \param len Input length.
     * \return 32-bit hash value.
     */
    inline uint32_t fnv1a_hash_32(const uint8_t* data, size_t len) {
        uint32_t hash = hash::FNV1A_OFFSET_BASIS_32;
        for (size_t i = 0; i < len; i++) {
            hash ^= data[i];
            hash *= hash::FNV1A_PRIME_32;
        }
        return hash;
    }
    ```

3. **Update each file** to use the centralized constants:
    ```cpp
    // TropicSlotMap.cpp
    #include "cdc_core/hash_constants.h"
    
    uint32_t TropicSlotMap::computeMapSignature() const {
        uint32_t hash = cdc::core::hash::FNV1A_OFFSET_BASIS_32;
        auto mix = [&hash](uint32_t v) {
            hash ^= v;
            hash *= cdc::core::hash::FNV1A_PRIME_32;
        };
        // ...
    }
    
    // vcard_store.cpp
    #include "cdc_core/hash_constants.h"
    
    static uint32_t fnv1a_hash(const char* data, size_t len) {
        uint32_t hash = cdc::core::hash::FNV1A_OFFSET_BASIS_32;
        for (size_t i = 0; i < len; i++) {
            hash ^= static_cast<uint8_t>(data[i]);
            hash *= cdc::core::hash::FNV1A_PRIME_32;
        }
        return hash;
    }
    ```

## References
- [FNV-1a Hash Algorithm](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash)
- [IETF FNV Draft](https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-1a)
- [FNV Constants](http://www.isthe.com/chongo/tech/comp/fnv/)
