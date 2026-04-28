---
title: "[MEDIUM] Shotgun Surgery: Adding a new module requires changes in many scattered locations"
severity: MEDIUM
domain: architecture
lens: code-smells
labels:
  - "shotgun-surgery"
  - "modules"
---

## Summary
Adding a new module to the CDC Badge OS requires changes in multiple scattered locations:
1. Create component folder in `components/<module_name>/`
2. Add to `main/CMakeLists.txt` MODULES list
3. Implement `<module_name>_register()` function
4. Add to `main/src/modules_init.c` if needed
5. Potentially add strings to `I18n::initCoreStrings()`
6. Update documentation

While the module system is fairly well-structured, the registration process touches many files.

## Impact
**High friction**: Adding a simple module requires editing 5+ files.

**Error-prone**: Easy to forget a step (e.g., forgetting to add to CMakeLists.txt).

**Inconsistent**: Different modules may follow slightly different patterns.

## Evidence
Based on examining the module structure:

1. **Component creation**: `components/mod_totp/`, `components/mod_fido2/`, `components/mod_password/` each have their own structure.

2. **CMakeLists.txt modification**: `main/CMakeLists.txt` has a MODULES list that must be updated.

3. **Registration function**: Each module has a `mod_<name>_register()` function (e.g., `mod_fido2_register()` in `mod_fido2_register()`).

4. **Module initialization**: `components/cdc_core/src/ModuleRegistry.cpp` must call the registration function.

5. **String registration**: New UI strings often go into `I18n::initCoreStrings()`.

Example from `components/mod_fido2/src/Fido2Module.cpp:290-297`:
```cpp
extern "C" void mod_fido2_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_fido2::Fido2Module::instance();
        if (module.init()) {
            module.start();
        }
    });
}
```

## Recommended Fix
1. **Automate module discovery**: Use CMake's `glob` to automatically find and include all modules in `components/` that have a `mod_*` naming pattern.

2. **Create module template**: Provide a script or template that generates the complete module structure:
```bash
./scripts/new_module.sh my_module
```

3. **Centralize registration**: Have a single `main/src/modules_init.c` that auto-registers all modules via function pointers in a table.

4. **Add module validation**: Create a build-time check that ensures all required files exist for a module.

**Estimated effort**: ~1 hour to create a basic module generation script and update CMakeLists.txt to use glob.

## References
- Refactoring.com: "Shotgun Surgery" - https://refactoring.com/catalog/combineMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
