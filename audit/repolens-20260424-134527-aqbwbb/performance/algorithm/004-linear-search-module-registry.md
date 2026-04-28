---
title: "[MEDIUM] Repeated linear search in ModuleRegistry operations"
severity: MEDIUM
domain: cdc_core
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/cdc_core/src/ModuleRegistry.cpp`, multiple functions use linear search through the modules array:
- `getModule()` (lines 117-130)
- `getModuleIndex()` (lines 100-110)
- `unregisterModule()` (lines 96-109)
- `clearModuleError()` (lines 678-692)

Each performs O(n) search with `strcmp()` for name matching.

**Evidence:**
```cpp
IModule* ModuleRegistry::getModule(const char* name) {
    if (!name) return nullptr;

    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            return modules_[i];
        }
    }
    return nullptr;
}
```

## Impact
- **Frequent lookups**: Module lookups occur during UI rendering, command execution, and slot allocation.
- **Compound operations**: Some operations call `getModule()` multiple times in sequence.
- **Total cost**: O(n × k) per lookup, where n is number of modules and k is average name length.

## Evidence
**File**: `components/cdc_core/src/ModuleRegistry.cpp`
**Lines**: 
- `getModule()`: 117-130
- `getModuleIndex()`: 100-110
- `unregisterModule()`: 96-109
- `clearModuleError()`: 678-692

## Recommended Fix
Add a hash table for O(1) module lookups. Since modules are registered once and looked up frequently:

```cpp
// In ModuleRegistry.h
#define MODULE_HASH_SIZE 64

struct ModuleEntry {
    IModule* module;
    uint32_t hash;
};

ModuleEntry moduleHash_[MODULE_HASH_SIZE];
uint8_t moduleCount_ = 0;

// Hash function (same as ServiceRegistry)
static inline uint32_t hashModuleName(const char* name) {
    uint32_t h = 5381;
    while (*name) {
        h = ((h << 5) + h) + *name++;
    }
    return h & (MODULE_HASH_SIZE - 1);
}

// O(1) lookup during registration
bool ModuleRegistry::registerModule(IModule* module) {
    // ... existing duplicate check using hash table ...
    modules_[count_++] = module;
    // Add to hash table
    uint32_t idx = hashModuleName(module->getName());
    moduleHash_[idx].module = module;
    moduleHash_[idx].hash = idx;
    return true;
}

// O(1) lookup
IModule* ModuleRegistry::getModule(const char* name) {
    uint32_t idx = hashModuleName(name);
    if (moduleHash_[idx].module && 
        strcmp(moduleHash_[idx].module->getName(), name) == 0) {
        return moduleHash_[idx].module;
    }
    return nullptr;
}
```

## References
- [Hash table for fast lookups](https://en.wikipedia.org/wiki/Hash_table)
