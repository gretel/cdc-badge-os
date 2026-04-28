---
title: "[LOW] Inconsistent naming for module registration functions"
severity: LOW
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
Module registration functions use inconsistent naming patterns:

**Current patterns found:**
- `mod_fido2_register()` - module name + "_register"
- `mod_gpg_register()` - module name + "_register"
- `mod_totp_register()` - module name + "_register"
- `mod_password_register()` - module name + "_register"

But the **registration macro** in `ModuleRegistry.h` uses different naming:
```cpp
#define CDC_MODULE(name) cdc::core::ModuleRegistry::instance().getModule(name)
```

And the **registration call** pattern is:
```cpp
extern "C" void mod_fido2_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_fido2::Fido2Module::instance();
        // ...
    });
}
```

No central documentation defines the expected pattern.

## Impact
- **Confusion**: New modules may use different patterns
- **Discovery**: Hard to find all module registration functions
- **Automation**: Build scripts may not work consistently

## Evidence
- FIDO2: components/mod_fido2/src/Fido2Module.cpp:288
- GPG: components/mod_gpg/src/GpgModule.cpp:656
- No central documentation of the pattern

## Recommended Fix
Document the standard pattern in `ModuleRegistry.h`:

```cpp
/**
 * \brief Module registration convention.
 *
 * Each module must define a registration function following this pattern:
 *
 *   extern "C" void mod_<snake_case_name>_register();
 *
 * Example:
 *   components/mod_fido2/src/Fido2Module.cpp:
 *   extern "C" void mod_fido2_register() {
 *       cdc::core::ModuleRegistry::instance().registerInitializer([]() {
 *           auto& module = cdc::mod_fido2::Fido2Module::instance();
 *           if (module.init()) {
 *               module.start();
 *           }
 *       });
 *   }
 *
 * Naming rules:
 * - Use "mod_" prefix
 * - Use snake_case for module name
 * - Suffix with "_register()"
 * - Must be extern "C" for C linkage
 *
 * Registration order:
 * - Functions are called in registration order
 * - Modules can depend on previously registered modules
 */
```

## References
- ModuleRegistry: components/cdc_core/include/cdc_core/ModuleRegistry.h
- FIDO2 example: components/mod_fido2/src/Fido2Module.cpp
