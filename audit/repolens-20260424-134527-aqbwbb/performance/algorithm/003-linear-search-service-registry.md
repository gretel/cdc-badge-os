---
title: "[MEDIUM] Repeated linear search in ServiceRegistry lookup"
severity: MEDIUM
domain: cdc_core
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/cdc_core/src/ServiceRegistry.cpp` (lines 48-79), the `getService()` function performs a linear search through the services array. This is called potentially many times during runtime to look up services by name.

**Evidence:**
```cpp
IService* ServiceRegistry::getService(const char* name) {
    if (!name) return nullptr;

    for (size_t i = 0; i < count_; i++) {
        if (strcmp(services_[i].name, name) == 0) {
            return services_[i].service;
        }
    }
    return nullptr;
}
```

Same pattern exists in `registerService()` (lines 50-55) for duplicate checking.

## Impact
- **Repeated lookups**: If `getService()` is called frequently (e.g., in event loops, UI updates), the linear search compounds.
- **String comparisons**: Each lookup performs `strcmp()` which is O(k) where k is the name length.
- **Total cost**: O(n × k) per lookup, where n is number of services and k is average name length.

## Evidence
**File**: `components/cdc_core/src/ServiceRegistry.cpp`
**Lines**: 48-79 (getService), 38-55 (registerService)
**Context**: Both `registerService()` and `getService()` use linear search through the services array.

## Recommended Fix
Use a hash table for O(1) average-case lookup. Since the maximum number of services is limited (`MAX_SERVICES`), a simple open-addressing hash table would work well:

```cpp
// In ServiceRegistry.h
#define SERVICE_HASH_SIZE 32  // Power of 2 for fast modulo

struct ServiceEntry {
    const char* name;
    IService* service;
};

ServiceEntry hashTable_[SERVICE_HASH_SIZE];

// Hash function
static inline uint32_t hashServiceName(const char* name) {
    uint32_t h = 5381;
    while (*name) {
        h = ((h << 5) + h) + *name++;  // djb2 hash
    }
    return h & (SERVICE_HASH_SIZE - 1);
}

// O(1) lookup
IService* ServiceRegistry::getService(const char* name) {
    uint32_t idx = hashServiceName(name);
    for (int i = 0; i < SERVICE_HASH_SIZE; i++) {
        uint32_t probe = (idx + i) & (SERVICE_HASH_SIZE - 1);
        if (!hashTable_[probe].name) return nullptr;  // Empty slot
        if (strcmp(hashTable_[probe].name, name) == 0) {
            return hashTable_[probe].service;
        }
    }
    return nullptr;
}
```

Alternatively, if the service count is small (<10) and lookups are infrequent, document the current implementation as acceptable.

## References
- [Hash table implementation](https://en.wikipedia.org/wiki/Hash_table)
- [DJB2 hash function](http://www.cse.yorku.ca/~oz/hash.html)
