---
title: "[MEDIUM] Test doesn't verify module actually registers correctly"
severity: MEDIUM
domain: testing/test-quality
lens: module-registration
labels:
  - "test-quality"
  - "module-registration"
  - "integration"
---

## Summary

The test `test/test_vcard_module_link/test_vcard_module_link.cpp` calls `mod_vcard_register()` but **doesn't verify** that the module was actually registered with the `ModuleRegistry`. 

The registration function is complex - it involves:
1. Creating a `VcardModule` instance
2. Registering i18n strings
3. Registering serial commands
4. Registering with `ModuleRegistry::instance()`

The test only checks that the function doesn't crash, not that any of these steps succeeded.

## Impact

**Undetected registration failures:**
- If `ModuleRegistry::registerModule()` fails silently, the test won't catch it
- If i18n string registration fails, the test won't know
- If serial commands aren't registered, the test won't verify

**Weak integration testing:**
- The module may appear "linked" but not actually work
- Missing verification of the full registration flow
- No check that the module is discoverable after registration

**False confidence:**
- Test passes even if module is in a broken state
- Developers may assume module is working when it's not fully registered

## Evidence

**File: `test/test_vcard_module_link/test_vcard_module_link.cpp`**
```cpp
#include "mod_vcard/VcardModule.h"

extern "C" void mod_vcard_register();

void test_vcard_module_link() {
    mod_vcard_register();  // Called, but result not verified
}
```

**Actual registration implementation** (`components/mod_vcard/src/VcardModule.cpp:554-560`):
```cpp
extern "C" void mod_vcard_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        static VcardModule instance;
        core::ModuleRegistry::instance().registerModule(&instance);
    });
}
```

**What should be verified:**
1. Module is in `ModuleRegistry` after registration
2. Module can be retrieved by name
3. Module's `getName()` returns correct value
4. Serial commands are registered (optional integration test)

## Recommended Fix

**Option 1: Verify module is in registry**

```cpp
#include "mod_vcard/VcardModule.h"
#include "cdc_core/ModuleRegistry.h"

extern "C" void mod_vcard_register();

void test_vcard_module_link() {
    // Register the module
    mod_vcard_register();
    
    // Trigger initialization (if needed)
    cdc::core::ModuleRegistry::instance().initialize();
    
    // Verify module is registered
    auto* module = cdc::core::ModuleRegistry::instance().getModule("vcard");
    assert(module != nullptr);
    assert(strcmp(module->getName(), "vcard") == 0);
}
```

**Option 2: Check module count**

```cpp
void test_vcard_module_link() {
    int count_before = cdc::core::ModuleRegistry::instance().getModuleCount();
    mod_vcard_register();
    cdc::core::ModuleRegistry::instance().initialize();
    int count_after = cdc::core::ModuleRegistry::instance().getModuleCount();
    
    assert(count_after == count_before + 1);
}
```

**Option 3: Verify module functionality**

```cpp
void test_vcard_module_link() {
    mod_vcard_register();
    cdc::core::ModuleRegistry::instance().initialize();
    
    auto* module = cdc::core::ModuleRegistry::instance().getModule("vcard");
    assert(module != nullptr);
    
    // Verify module can be cast to VcardModule
    auto* vcard = static_cast<mod_vcard::VcardModule*>(module);
    assert(vcard != nullptr);
    
    // Optional: Verify serial commands are registered
    // (would need access to CommandRegistry)
}
```

## References

- [Module Development Guide](docs/MODULE_DEVELOPMENT.md) - Module registration pattern
- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Registry API
