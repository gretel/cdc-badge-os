---
title: "[MEDIUM] C-style wrapper functions add unnecessary coupling layer between GPG and PinManager"
severity: MEDIUM
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
The `pin_storage.cpp` file creates a C-style wrapper layer that directly delegates to `PinManager::instance()`. This adds an unnecessary indirection where modules could directly use `PinManager`. The wrapper creates coupling without providing abstraction benefits.

**Evidence locations:**
- `components/mod_gpg/src/pin_storage.cpp` - Wrapper functions delegating to PinManager singleton
- `components/mod_gpg/src/GpgModule.cpp:274-333` - GPG module calls pin_storage functions

## Impact
- **Double coupling**: GPG module → pin_storage → PinManager singleton
- **No abstraction benefit**: Wrapper just passes through to same singleton
- **Maintenance overhead**: Changes to PinManager API require updating wrappers
- **Confusing layering**: Unclear why wrapper exists vs direct PinManager access

## Evidence
```cpp
// components/mod_gpg/src/pin_storage.cpp
#include "pin_storage.h"
#include "cdc_core/PinManager.h"

// Line 4-6: Wrapper just calls singleton
void pin_storage_openpgp_init(void) {
    cdc::core::PinManager::instance().init();
}

// Line 8-10: Another wrapper
bool pin_storage_openpgp_verify_pw1(const char *pin) {
    return cdc::core::PinManager::instance().verifyPW1(pin);
}

// Line 16-18: More wrappers
bool pin_storage_openpgp_change_pw1(const char *new_pin) {
    return cdc::core::PinManager::instance().setPW1(new_pin);
}

// All 13 functions follow same pattern - direct delegation to PinManager singleton
```

```cpp
// components/mod_gpg/src/GpgModule.cpp:274-333
// Module calls wrapper functions
static bool gpg_verify_pw1(const char* pin) {
    return pin_storage_openpgp_verify_pw1(pin);  // Calls wrapper
}

static bool gpg_change_pw1(const char*, const char* newPin) {
    return pin_storage_openpgp_change_pw1(newPin);  // Calls wrapper
}
```

**Coupling chain:**
```
GpgModule → pin_storage_openpgp_*() → PinManager::instance()
```

**Direct access would be:**
```
GpgModule → PinManager::instance()
```

## Recommended Fix
**Remove wrapper layer, use PinManager directly:**

1. **Option 1: Remove pin_storage.cpp entirely**
```cpp
// components/mod_gpg/src/GpgModule.cpp
// Before:
static bool gpg_verify_pw1(const char* pin) {
    return pin_storage_openpgp_verify_pw1(pin);
}

// After:
static bool gpg_verify_pw1(const char* pin) {
    return cdc::core::PinManager::instance().verifyPW1(pin);
}
```

2. **Option 2: Convert to C++ class with dependency injection**
```cpp
// components/mod_gpg/include/mod_gpg/PinStorage.h
class PinStorage {
public:
    explicit PinStorage(cdc::core::PinManager* pm) : pinManager_(pm) {}
    
    void init() { pinManager_->init(); }
    bool verifyPW1(const char* pin) { return pinManager_->verifyPW1(pin); }
    bool changePW1(const char* newPin) { return pinManager_->setPW1(newPin); }
    // ...
    
private:
    cdc::core::PinManager* pinManager_;
};

// components/mod_gpg/src/GpgModule.cpp
class GpgModule {
public:
    GpgModule() : pinStorage_(&cdc::core::PinManager::instance()) {}
    
private:
    PinStorage pinStorage_;
};

static bool gpg_verify_pw1(GpgModule& module, const char* pin) {
    return module.pinStorage_.verifyPW1(pin);
}
```

3. **If C API needed for legacy code, make it a thin macro layer:**
```cpp
// components/mod_gpg/include/pin_storage.h
#define pin_storage_openpgp_init() \
    cdc::core::PinManager::instance().init()
#define pin_storage_openpgp_verify_pw1(pin) \
    cdc::core::PinManager::instance().verifyPW1(pin)
// ... etc
```

## References
- [Facade Pattern](https://en.wikipedia.org/wiki/Facade_pattern)
- [Layered Architecture](https://en.wikipedia.org/wiki/Layered_architecture)
