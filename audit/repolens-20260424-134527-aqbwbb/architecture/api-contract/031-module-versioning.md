---
title: "[LOW] Missing Explicit Module Versioning Contract for API Compatibility"
severity: LOW
domain: architecture/api-contract
lens: versioning-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
Modules have a `getVersion()` method that returns a string, but there is no structured versioning contract (semantic versioning, compatibility checking) to ensure modules can work together when APIs change.

## Impact
- **Silent API breaks**: When `IModule` interface changes, old modules may not compile or may crash at runtime.
- **No compatibility checking**: System doesn't verify module versions before loading.
- **Hard to track changes**: Version strings are opaque ("1.0" vs "2.0") with no semantic meaning.
- **Upgrade uncertainty**: No way to know if a module is compatible with current firmware version.

## Evidence
**Version method in IModule - `components/cdc_core/include/cdc_core/IModule.h:73-75`:**
```cpp
/**
 * Get module version string
 */
virtual const char* getVersion() const = 0;
```

**Simple version implementations:**
```cpp
// mod_totp/src/TotpModule.cpp:15
const char* TotpModule::getVersion() const override { return "1.0"; }

// mod_hid/src/HidModule.cpp:25
const char* HidModule::getVersion() const override { return "1.0"; }
```

**No version checking in ModuleRegistry:**
```cpp
// ModuleRegistry only checks name and slot requirements
bool ModuleRegistry::registerModule(IModule* module) {
    // ... name check, slot check ...
    // No version compatibility check!
    modules_[count_++] = module;
    return true;
}
```

**No semantic versioning structure:**
```cpp
// Current: opaque string
virtual const char* getVersion() const = 0;

// Missing: structured version
struct ModuleVersion {
    uint8_t major;    // Breaking changes
    uint8_t minor;    // New features
    uint8_t patch;    // Bug fixes
};
virtual ModuleVersion getVersion() const = 0;
```

## Recommended Fix
**Implement semantic versioning contract:**

1. **Update IModule with structured version:**
```cpp
namespace cdc::core {

struct ModuleVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    
    uint32_t toUint32() const {
        return (major << 16) | (minor << 8) | patch;
    }
    
    bool operator==(const ModuleVersion& other) const {
        return toUint32() == other.toUint32();
    }
    
    bool operator>=(const ModuleVersion& other) const {
        return toUint32() >= other.toUint32();
    }
};

class IModule : public IService {
public:
    virtual ModuleVersion getVersion() const = 0;
    virtual ModuleVersion getMinFirmwareVersion() const { return {0, 0, 0}; }
};

} // namespace cdc::core
```

2. **Add compatibility checking in ModuleRegistry.**

3. **Update modules to use structured version.**

4. **Add version checking at startup in main.cpp.**

## References
- IModule: `components/cdc_core/include/cdc_core/IModule.h:73-75`
- ModuleRegistry: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
