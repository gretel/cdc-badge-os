---
title: "[LOW] Duplicated module registration boilerplate across all modules"
severity: LOW
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
All 10 modules in the codebase have nearly identical registration boilerplate code that could be extracted into a reusable macro or helper function. Each module implements the same pattern for registering with the ModuleRegistry.

**Locations:**
- `components/mod_fido2/src/Fido2Module.cpp:290`
- `components/mod_gpg/src/GpgModule.cpp:654`
- `components/mod_totp/src/TotpModule.cpp:1013`
- `components/mod_password/src/PasswordModule.cpp:849`
- `components/mod_sao/src/SaoModule.cpp:91`
- `components/mod_hid/src/HidModule.cpp:373`
- `components/mod_vcard/src/VcardModule.cpp:554`
- `components/mod_ble_serial/src/BleSerialModule.cpp:326`
- `components/mod_nvsedit/src/NvsEditModule.cpp:589`
- `components/grove_led/src/GroveLedModule.cpp:635`

## Impact
**Code Duplication:**
- ~10 identical registration functions with only module name/type differences
- Any change to registration pattern requires updating all 10 modules
- Increased codebase size without adding value

**Maintenance Burden:**
- Bug fixes to registration logic must be applied in multiple places
- Risk of divergence as modules evolve independently

## Evidence
**Identical pattern in all modules:**

```cpp
// mod_fido2/src/Fido2Module.cpp:290
extern "C" void mod_fido2_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_fido2::Fido2Module::instance();
        if (module.init()) {
            module.start();
        }
    });
}

// mod_gpg/src/GpgModule.cpp:654 (virtually identical)
extern "C" void mod_gpg_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_gpg::GpgModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}

// mod_sao/src/SaoModule.cpp:91 (virtually identical)
extern "C" void mod_sao_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_sao::SaoModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}
```

**Variations:**
- `mod_ble_serial` and `grove_led` include explicit `registerModule()` call
- `mod_nvsedit` uses `s_module` instead of `::instance()`

## Recommended Fix
Extract the common registration pattern into a macro or helper function:

**Option 1: Define a registration macro**

Create `components/cdc_core/include/cdc_core/ModuleRegistration.h`:
```cpp
#ifndef CDC_CORE_MODULE_REGISTRATION_H
#define CDC_CORE_MODULE_REGISTRATION_H

#include "ModuleRegistry.h"

#define CDC_MODULE_REGISTER(MODULE_NAME, MODULE_CLASS, MODULE_NS) \
    extern "C" void MODULE_NAME##_register() { \
        cdc::core::ModuleRegistry::instance().registerInitializer([]() { \
            auto& module = cdc::MODULE_NS::MODULE_CLASS::instance(); \
            if (module.init()) { \
                module.start(); \
            } \
        }); \
    }

#define CDC_MODULE_REGISTER_WITH_INIT(MODULE_NAME, MODULE_CLASS, MODULE_NS) \
    extern "C" void MODULE_NAME##_register() { \
        auto& moduleReg = cdc::core::ModuleRegistry::instance(); \
        auto& module = cdc::MODULE_NS::MODULE_CLASS::instance(); \
        moduleReg.registerModule(&module); \
        if (!module.init()) { \
            moduleReg.reportModuleError(module.getName(), "Init failed"); \
            return; \
        } \
        module.start(); \
    }

#endif
```

**Option 2: Create a registration helper function**

```cpp
// cdc_core/src/ModuleRegistration.cpp
namespace cdc::core {

template<typename ModuleType>
void registerModuleWithInit() {
    ModuleRegistry::instance().registerInitializer([]() {
        auto& module = ModuleType::instance();
        if (module.init()) {
            module.start();
        }
    });
}

} // namespace cdc::core
```

**Usage in modules:**
```cpp
// mod_fido2/src/Fido2Module.cpp
#include "cdc_core/ModuleRegistration.h"

CDC_MODULE_REGISTER(mod_fido2, Fido2Module, mod_fido2)
```

## References
- DRY principle (Don't Repeat Yourself): https://en.wikipedia.org/wiki/Don%27t_repeat_yourself
- Macro for boilerplate reduction: https://en.wikipedia.org/wiki/Macro_(computer_science)
- Existing project pattern: `main/CMakeLists.txt` already auto-generates registration calls
