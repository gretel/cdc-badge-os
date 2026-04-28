---
title: "[HIGH] Missing explicit interface for module-to-module communication"
severity: HIGH
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
Modules access each other directly via singleton instances without explicit interface definitions, creating fragile implicit contracts. Examples:

1. **GPG module accesses PinManager directly** (components/mod_gpg/src/GpgModule.cpp:15-16):
```cpp
#include "cdc_core/PinManager.h"
#include "pin_storage.h"
```
And calls: `cdc::core::PinManager::PW1_MIN`, `cdc::core::PinManager::PIN_MAX`

2. **FIDO2 module accesses UsbManager directly** (components/mod_fido2/src/Fido2Module.cpp:3):
```cpp
#include "cdc_core/UsbManager.h"
```

3. **PasswordStore accesses ISecureElement** (components/mod_password/include/mod_password/PasswordStore.h:3):
```cpp
#include "cdc_hal/ISecureElement.h"
```

There is no explicit interface contract defining what data/methods are available between modules. Modules import concrete classes directly instead of using dependency injection or interface-based communication.

## Impact
- **Tight coupling**: Modules depend on concrete implementations, making testing and replacement difficult
- **Fragile changes**: Changing a method signature in `PinManager` breaks all modules that use it
- **No compile-time contract**: If `PinManager::PW1_MIN` changes type, only runtime errors occur
- **Circular dependency risk**: Modules can easily create circular dependencies
- **Hard to discover**: No central definition of what interfaces modules expose to each other

## Evidence
- GPG module includes `PinManager.h` and uses static constants (components/mod_gpg/src/GpgModule.cpp:362)
- FIDO2 module includes `UsbManager.h` directly (components/mod_fido2/src/Fido2Module.cpp:162)
- PasswordStore includes `ISecureElement.h` (components/mod_password/include/mod_password/PasswordStore.h:3)
- ServiceRegistry has `ServiceType` enum but only defines `KEYBOARD, CLIPBOARD, NOTIFICATION` (components/cdc_core/include/cdc_core/ServiceRegistry.h:12-16)

## Recommended Fix
Establish explicit inter-module interfaces:

1. **Create `IModuleRegistry` interface** in `components/cdc_core/include/cdc_core/ModuleRegistry.h`:
```cpp
class IModuleRegistry {
public:
    virtual ~IModuleRegistry() = default;
    virtual cdc::core::IModule* getModule(const char* name) = 0;
};
```

2. **Define module-specific interfaces** (e.g., `components/cdc_core/include/cdc_core/IGpgService.h`):
```cpp
class IGpgService {
public:
    virtual ~IGpgService() = default;
    virtual bool hasKey() const = 0;
    virtual bool sign(const uint8_t* hash, uint8_t* sig) = 0;
};
```

3. **Register services with ServiceRegistry**:
```cpp
enum class ServiceType {
    KEYBOARD,
    CLIPBOARD,
    NOTIFICATION,
    GPG,      // Add here
    FIDO2,    // Add here
    TOTP,     // Add here
};
```

4. **Modules provide/request services**:
```cpp
// In GpgModule::start()
CDC_PROVIDE_SERVICE(ServiceType::GPG, &gpgService);

// In another module
auto* gpg = CDC_REQUEST_SERVICE(ServiceType::GPG, cdc::core::IGpgService);
```

## References
- ServiceRegistry: components/cdc_core/include/cdc_core/ServiceRegistry.h
- IModule interface: components/cdc_core/include/cdc_core/IModule.h
- GPG module: components/mod_gpg/src/GpgModule.cpp
