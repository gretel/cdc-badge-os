---
title: "[MEDIUM] Inconsistent module registration timing in initializer functions"
severity: MEDIUM
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

Modules use two different patterns for when they register themselves with the `ModuleRegistry`:

1. **Registration in init()** (most modules): Module registers itself during `init()` call
2. **Registration before init()** (Grove LED): Module registers itself before calling `init()`

This affects when the module becomes visible in the UI and how errors are reported.

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp` - Registers in `init()`
- `components/mod_gpg/src/GpgModule.cpp` - Registers in `init()`
- `components/mod_fido2/src/Fido2Module.cpp` - Registers in `init()`
- `components/mod_password/src/PasswordModule.cpp` - Registers in `init()`
- `components/grove_led/src/GroveLedModule.cpp` - Registers BEFORE `init()`

### Standard pattern (most modules)

```cpp
// In mod_totp/src/TotpModule.cpp
bool TotpModule::init() {
    LOG_I(TAG, "Initializing TOTP module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);  // Register during init
    if (slotRange_.hasRmem) {
        TotpStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "TOTP slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

extern "C" void mod_totp_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_totp::TotpModule::instance();
        if (module.init()) {  // Register happens inside init()
            module.start();
        }
    });
}
```

### Pre-registration pattern (Grove LED)

```cpp
// In grove_led/src/GroveLedModule.cpp
extern "C" void grove_led_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        auto& module = cdc::grove_led::GroveLedModule::instance();

        moduleReg.registerModule(&module);  // Register BEFORE init!

        if (!module.init()) {
            // Init failed - report error
            moduleReg.reportModuleError(module.getName(), "LED strip init failed");
            return;
        }

        module.start();
    });
}
```

## Impact

1. **UI visibility timing**: Grove LED appears in menu even if init fails; other modules don't
2. **Error reporting**: Grove LED uses `reportModuleError()` to mark failed init; others return false and module stays unregistered
3. **Slot validation**: Grove LED registers before slot range is set; others register after slot validation
4. **Menu ordering**: Registration order affects menu item ordering; Grove LED is registered earlier than others

### Behavior comparison

| Module | Visible if init fails? | Error shown? | Menu order |
|--------|-----------------------|--------------|------------|
| TOTP | No | Toast error | Later |
| GPG | No | Toast error | Later |
| FIDO2 | No | Toast error | Later |
| Grove LED | Yes | Module error status | Earlier |

## Evidence

**Standard pattern (registration in init):**
- `components/mod_totp/src/TotpModule.cpp:934` - `core::ModuleRegistry::instance().registerModule(this);`
- `components/mod_gpg/src/GpgModule.cpp:555` - `core::ModuleRegistry::instance().registerModule(this);`
- `components/mod_fido2/src/Fido2Module.cpp:130` - `core::ModuleRegistry::instance().registerModule(this);`
- `components/mod_password/src/PasswordModule.cpp:765` - `core::ModuleRegistry::instance().registerModule(this);`

**Pre-registration pattern (registration before init):**
- `components/grove_led/src/GroveLedModule.cpp:641` - `moduleReg.registerModule(&module);` (before `module.init()`)

**ModuleRegistry behavior:**
- `components/cdc_core/include/cdc_core/ModuleRegistry.h:174` - `void reportModuleError(const char* name, const char* message);`
- `components/cdc_core/include/cdc_core/ModuleRegistry.h:53` - `bool registerModule(IModule* module);`

## Recommended Fix

### Option 1: Standardize on registration in init() (recommended)

Update Grove LED to match the standard pattern:

```cpp
// In grove_led/src/GroveLedModule.h
class GroveLedModule : public core::IModule {
public:
    // ...
    bool init() override;  // Will register itself here
    // ...
};

// In grove_led/src/GroveLedModule.cpp
bool GroveLedModule::init() {
    LOG_I(TAG, "Initializing Grove LED module");

    // Register i18n strings first
    registerStrings();

    loadSettings();

    // Configure LED strip
    // ...

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_);
    if (err != ESP_OK) {
        LOG_E(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
        return false;  // Don't register if init fails
    }

    // Register module (after successful hardware init)
    core::ModuleRegistry::instance().registerModule(this);

    clearLeds();
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

extern "C" void grove_led_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::grove_led::GroveLedModule::instance();
        if (module.init()) {  // Registration happens inside init()
            module.start();
        }
    });
}
```

### Option 2: Document the pre-registration pattern

If pre-registration is intentional (e.g., to always show in menu), document it:

```cpp
/**
 * \brief Module registration patterns:
 * 
 * 1. Standard (most modules):
 *    - Register in init() after validation
 *    - Module hidden if init fails
 *    - Example: TotpModule, GpgModule
 * 
 * 2. Pre-registration (Grove LED):
 *    - Register before init() for always-visible
 *    - Use reportModuleError() for failures
 *    - Use when module should appear even if temporarily disabled
 */
```

### Option 3: Use isVisible() callback

Instead of pre-registering, use the `isVisible()` callback:

```cpp
items[0] = {
    .label = mstr(STR_GROVE_LED),
    .priority = 50,
    .getView = getGroveLedMenu,
    .isVisible = []() { return GroveLedModule::instance().getState() != core::ServiceState::ERROR; },
    .moduleName = nullptr,
    .location = core::MenuLocation::TOOLS_MENU,
    .onSelect = nullptr
};
```

## References

- `components/cdc_core/include/cdc_core/ModuleRegistry.h` - Module registration API
- `components/mod_totp/src/TotpModule.cpp` - Reference implementation
- `components/grove_led/src/GroveLedModule.cpp` - Pre-registration example

</content>