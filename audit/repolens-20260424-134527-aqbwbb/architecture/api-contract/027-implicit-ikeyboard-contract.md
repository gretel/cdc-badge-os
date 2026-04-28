---
title: "[MEDIUM] Implicit IKeyboardProvider Contract - No Compile-Time Dependency Checking"
severity: MEDIUM
domain: architecture/api-contract
lens: service-dependency-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IKeyboardProvider` service contract between `mod_hid` (provider) and `mod_totp`/`mod_password` (consumers) is entirely implicit. There is no compile-time or runtime mechanism to verify that:
1. A provider exists when a consumer needs it
2. The provider implements all required methods
3. Dependencies are satisfied at module registration time

## Impact
- **Fragile module removal**: If `mod_hid` is removed from `CMakeLists.txt`, `mod_totp` and `mod_password` silently fail (no compile error, just runtime `nullptr` checks).
- **No documentation**: New developers don't know which modules depend on which services.
- **Inconsistent behavior**: Modules may work or fail depending on configuration, making debugging difficult.

## Evidence
**Service registration (no contract enforcement) - `components/mod_hid/src/HidModule.cpp:313-319`:**
```cpp
// Register keyboard service for other modules
core::ServiceRegistry::instance().provide<core::IKeyboardProvider>(
    core::ServiceType::KEYBOARD,
    &BleHidKeyboard::instance()
);
```

**Service consumption (no compile-time check) - `components/mod_totp/src/TotpModule.cpp:463`:**
```cpp
auto* kb = core::getKeyboard();  // Returns nullptr if not registered
if (kb && kb->isConnected()) {
    kb->typeString(code_);
}
```

**Service interface definition - `components/cdc_core/include/cdc_core/IKeyboardProvider.h:23-70`:**
```cpp
class IKeyboardProvider {
public:
    virtual ~IKeyboardProvider() = default;
    virtual bool isConnected() const = 0;
    virtual bool typeString(const char* text, uint16_t delayMs = 50) = 0;
    virtual bool typeChar(char c) = 0;
    virtual bool isBusy() const = 0;
    virtual void cancel() = 0;
    virtual const char* getStatusText() const { return isConnected() ? "Connected" : "Disconnected"; }
};

IKeyboardProvider* getKeyboard();  // Returns nullptr if not registered
```

**No dependency tracking in IModule - `components/cdc_core/include/cdc_core/IModule.h:53-139`:**
```cpp
class IModule : public IService {
public:
    virtual const char* getName() const = 0;
    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    // ... no dependency declarations ...
};
```

## Recommended Fix
**Add explicit dependency declarations to IModule:**

1. **Update `IModule.h`:**
```cpp
struct ModuleDependency {
    ServiceType serviceType;
    const char* serviceName;
    bool required;  // true = fail if missing, false = optional
};

class IModule : public IService {
public:
    virtual ModuleDependency* getDependencies() { return nullptr; }
    virtual void onDependencyReady(ServiceType type) {}
    virtual void onDependencyMissing(ServiceType type) {}
};
```

2. **Update `mod_hid` to declare it provides the service.**

3. **Update `mod_totp` to declare it requires the service.**

4. **Update `ModuleRegistry` to check dependencies during initialization.**

## References
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- ServiceRegistry: `components/cdc_core/include/cdc_core/ServiceRegistry.h`
- ModuleRegistry: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
