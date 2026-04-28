---
title: "[HIGH] Module Initialization Order Race Condition - IKeyboardProvider Service Not Available"
severity: HIGH
domain: architecture/api-contract
lens: module-initialization-order
labels:
  - "audit:architecture/api-contract"
---

## Summary
The TOTP and Password modules are initialized BEFORE the HID module, but they depend on the `IKeyboardProvider` service that is only registered by `mod_hid`. This creates a race condition where modules may try to use a service that doesn't exist yet.

**Module Initialization Order (from `main/CMakeLists.txt`):**
1. `mod_totp` (line 10)
2. `mod_password` (line 12)
3. `mod_hid` (line 18)

**Service Registration (from `mod_hid/src/HidModule.cpp:316`):**
```cpp
core::ServiceRegistry::instance().provide<core::IKeyboardProvider>(
    core::ServiceType::KEYBOARD,
    &BleHidKeyboard::instance()
);
```

## Impact
- **Runtime failures**: When TOTP or Password modules try to use `getKeyboard()` or `request<IKeyboardProvider>()`, they get `nullptr` because `mod_hid` hasn't initialized yet.
- **Silent failures**: Code checks `if (kb && kb->isConnected())` but the keyboard might become available later, causing inconsistent behavior.
- **User experience**: Auto-type feature works only if modules are initialized in the right order, which is not documented or enforced.

## Evidence
**Module order in `main/CMakeLists.txt` (lines 8-19):**
```cmake
set(MODULES
    grove_led
    mod_totp          # Line 10 - initialized first
    mod_fido2
    mod_password      # Line 12 - initialized second
    mod_gpg
    mod_sao
    mod_vcard
    mod_ble_serial
    mod_nvsedit
    mod_hid           # Line 18 - initialized last
)
```

**TOTP module usage (from `components/mod_totp/src/TotpModule.cpp:464`):**
```cpp
if (key == 'Y') {
    auto* kb = core::getKeyboard();
    if (kb && kb->isConnected()) {
        kb->typeString(code_);
        ui::showToastSuccess("Typed");
    }
}
```

**Service registration in `components/mod_hid/src/HidModule.cpp:313-319`:**
```cpp
bool HidModule::init() {
    // ...
    // Register keyboard service for other modules
    core::ServiceRegistry::instance().provide<core::IKeyboardProvider>(
        core::ServiceType::KEYBOARD,
        &BleHidKeyboard::instance()
    );
    // ...
}
```

**Module registration flow (from `main/main.cpp:221-226`):**
```cpp
// Register all modules (configured in main/CMakeLists.txt)
modules_register_all();

// Run all registered module initializers
LOG_I(TAG, "Initializing modules...");
cdc::core::ModuleRegistry::instance().runAllInitializers();
```

## Recommended Fix
**Option 1: Enforce initialization order in CMakeLists.txt**
Move `mod_hid` BEFORE modules that depend on it:
```cmake
set(MODULES
    grove_led
    mod_hid           # Move to line 10 - before dependents
    mod_totp
    mod_fido2
    mod_password
    mod_gpg
    mod_sao
    mod_vcard
    mod_ble_serial
    mod_nvsedit
)
```

**Option 2: Add explicit dependency declarations**
Create a dependency graph in `IModule.h`:
```cpp
struct ModuleDependencies {
    const char* requires[MAX_DEPENDENCIES] = {};
    const char* provides[MAX_DEPENDENCIES] = {};
};
```

**Option 3: Lazy service lookup**
Change consumers to re-check service availability on each use instead of caching at init time.

## References
- Module Registry: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
- Service Registry: `components/cdc_core/include/cdc_core/ServiceRegistry.h`
- IKeyboardProvider: `components/cdc_core/include/cdc_core/IKeyboardProvider.h`
