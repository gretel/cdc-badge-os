---
title: "[LOW] Inconsistent use of extern \"C\" for module registration functions"
severity: LOW
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
Module registration functions use `extern "C"` inconsistently across headers:

**Correct usage** (extern "C" for registration):
```cpp
// components/mod_fido2/include/mod_fido2/Fido2Module.h
extern "C" void mod_fido2_register();

// components/mod_gpg/include/mod_gpg/GpgModule.h
extern "C" void mod_gpg_register();
```

**Missing extern "C"**:
```cpp
// components/mod_totp/include/mod_totp/TotpModule.h
// No registration function declaration visible here!
```

**Inconsistent placement**:
Some headers declare the registration function in the `.h` file, others in the `.cpp` file only.

## Impact
- **Linking issues**: Missing `extern "C"` causes C++ name mangling
- **Discovery difficulty**: Hard to find how modules register themselves
- **Inconsistent pattern**: New developers may not follow the convention

## Evidence
- FIDO2: components/mod_fido2/include/mod_fido2/Fido2Module.h:30
- GPG: components/mod_gpg/include/mod_gpg/GpgModule.h:30
- TOTP: components/mod_totp/include/mod_totp/TotpModule.h - no registration declaration

## Recommended Fix
Standardize module registration pattern:

1. **Always use `extern "C"`**:
```cpp
// In all module headers
extern "C" void mod_<name>_register();
```

2. **Document the pattern** (add to IModule.h or ModuleRegistry.h):
```cpp
/**
 * \brief Module registration convention.
 *
 * Each module must define a registration function:
 *   extern "C" void mod_<name>_register();
 *
 * This function is called by ModuleRegistry to initialize the module.
 * Use extern "C" to prevent C++ name mangling.
 *
 * Example:
 *   extern "C" void mod_fido2_register() {
 *       cdc::core::ModuleRegistry::instance().registerInitializer([]() {
 *           auto& module = cdc::mod_fido2::Fido2Module::instance();
 *           module.init();
 *           module.start();
 *       });
 *   }
 */
```

3. **Add to all module headers** (if missing):
```cpp
// components/mod_totp/include/mod_totp/TotpModule.h
extern "C" void mod_totp_register();
```

## References
- Module registration: components/cdc_core/include/cdc_core/ModuleRegistry.h
- FIDO2 example: components/mod_fido2/include/mod_fido2/Fido2Module.h
