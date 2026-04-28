---
title: "[LOW] Duplicated module lifecycle and registration boilerplate"
severity: LOW
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

All modules in the CDC Badge OS follow an identical pattern for lifecycle management (`init()`, `start()`, `stop()`) and registration. While this is intentional design for consistency, the boilerplate code is duplicated across 9+ modules:

- `mod_totp`, `mod_password`, `mod_gpg`, `mod_fido2`, `mod_vcard`, `mod_hid`, `mod_ble_serial`, `mod_nvsedit`, `mod_sao`

### Code Comparison

**`init()` in TotpModule.cpp (lines 928-946):**
```cpp
bool TotpModule::init() {
    LOG_I(TAG, "Initializing TOTP module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);
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
```

**`init()` in PasswordModule.cpp (lines 759-777):**
```cpp
bool PasswordModule::init() {
    LOG_I(TAG, "Initializing Password module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);
    if (slotRange_.hasRmem) {
        PasswordStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "Password slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    state_ = core::ServiceState::INITIALIZED;
    return true;
}
```

**`start()` in TotpModule.cpp (lines 950-958):**
```cpp
bool TotpModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}
```

**`start()` in PasswordModule.cpp (lines 781-789):**
```cpp
bool PasswordModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}
```

**Module registration (extern "C"):**
All modules use identical registration pattern:
```cpp
extern "C" void mod_<name>_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_<name>::<Module>Module::instance();
        if (module.init()) {
            module.start();
        }
    });
}
```

## Impact

- **Code Bloat:** Approximately 150-200 lines of duplicated boilerplate across all modules.
- **Maintenance:** Changes to module lifecycle must be applied across all modules.
- **Onboarding:** New developers must understand the pattern for each module.
- **Flexibility:** Adding new lifecycle stages requires changes to all modules.

## Evidence

**Files Affected (9 modules):**
1. `components/mod_totp/src/TotpModule.cpp`
2. `components/mod_password/src/PasswordModule.cpp`
3. `components/mod_gpg/src/GpgModule.cpp`
4. `components/mod_fido2/src/Fido2Module.cpp`
5. `components/mod_vcard/src/VcardModule.cpp`
6. `components/mod_hid/src/HidModule.cpp`
7. `components/mod_ble_serial/src/BleSerialModule.cpp`
8. `components/mod_nvsedit/src/NvsEditModule.cpp`
9. `components/mod_sao/src/SaoModule.cpp`

**Duplicated Patterns:**
1. `init()` method (lines ~928-946 in TOTP, ~759-777 in Password)
2. `start()` method (lines ~950-958 in TOTP, ~781-789 in Password)
3. `stop()` method (varies by module)
4. `getMenuItems()` method (varies by module)
5. `extern "C"` registration function (lines ~1013 in TOTP, ~849 in Password)

## Recommended Fix

### Option 1: Base Module Class

Create `components/cdc_core/include/cdc_core/ModuleBase.h`:

```cpp
#pragma once
#include "IModule.h"
#include "ServiceRegistry.h"
#include "cdc_log.h"

namespace cdc::core {

/**
 * \brief Base class for all modules with common lifecycle.
 */
class ModuleBase : public IModule {
public:
    ModuleBase(const char* name) : name_(name) {}
    virtual ~ModuleBase() = default;

    /**
     * \brief Common initialization boilerplate.
     * \param registerStrings Lambda to register i18n strings.
     * \param registerCommands Lambda to register serial commands.
     * \param configureSlots Lambda to configure slot range.
     * \return `true` if initialization succeeded.
     */
    bool initBase(std::function<void()> registerStrings,
                  std::function<void()> registerCommands,
                  std::function<bool()> configureSlots) {
        LOG_I(name_, "Initializing module");
        registerStrings();
        registerCommands();

        ModuleRegistry::instance().registerModule(this);
        if (configureSlots()) {
            ModuleRegistry::instance().clearModuleErrorByName(getName());
        } else {
            ModuleRegistry::instance().reportModuleError(getName(), "Slot range missing");
            state_ = ServiceState::ERROR;
            return false;
        }
        state_ = ServiceState::INITIALIZED;
        return true;
    }

    /**
     * \brief Common start transition.
     * \return `true` if start transition succeeded.
     */
    bool startBase() {
        if (state_ != ServiceState::INITIALIZED &&
            state_ != ServiceState::STOPPED) {
            return false;
        }
        state_ = ServiceState::STARTED;
        return true;
    }

    /**
     * \brief Common stop transition.
     */
    void stopBase() {
        state_ = ServiceState::STOPPED;
    }

    const char* getName() const override { return name_; }
    ServiceState getState() const override { return state_; }

protected:
    const char* name_;
    ServiceState state_ = ServiceState::STOPPED;
};

} // namespace cdc::core
```

### Option 2: Macro-Based Boilerplate

Create `components/cdc_core/include/cdc_core/ModuleMacros.h`:

```cpp
#pragma once

#define CDC_MODULE_INIT(ModuleClass, StoreClass, SlotType) \
bool ModuleClass::init() { \
    LOG_I(TAG, "Initializing " #ModuleClass " module"); \
    registerStrings(); \
    registerCommands(); \
    core::ModuleRegistry::instance().registerModule(this); \
    if (slotRange_.has##SlotType) { \
        StoreClass::instance().setSlotRange( \
            slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId); \
        core::ModuleRegistry::instance().clearModuleErrorByName(getName()); \
    } else { \
        core::ModuleRegistry::instance().reportModuleError(getName(), \
            #ModuleClass " slot range missing"); \
        state_ = core::ServiceState::ERROR; \
        return false; \
    } \
    state_ = core::ServiceState::INITIALIZED; \
    return true; \
}

#define CDC_MODULE_REGISTER(ModuleName, ModuleClass) \
extern "C" void mod_##ModuleName##_register() { \
    cdc::core::ModuleRegistry::instance().registerInitializer([]() { \
        auto& module = cdc::mod_##ModuleName::ModuleClass::instance(); \
        if (module.init()) { \
            module.start(); \
        } \
    }); \
}
```

### Implementation Steps (Option 1 - Recommended)

1. Create `components/cdc_core/include/cdc_core/ModuleBase.h`
2. Update each module to inherit from `ModuleBase`:
   ```cpp
   class TotpModule : public core::ModuleBase {
   public:
       TotpModule() : ModuleBase("TOTP") {}
       bool init() override {
           return initBase(
               []() { registerStrings(); },
               []() { registerCommands(); },
               []() {
                   if (slotRange_.hasRmem) {
                       TotpStore::instance().setSlotRange(...);
                       return true;
                   }
                   return false;
               }
           );
       }
   };
   ```
3. Remove duplicated `start()`, `stop()` methods
4. Use `startBase()`, `stopBase()` instead

## References

- [Template Method Pattern](https://en.wikipedia.org/wiki/Template_method_pattern)
- [CRTP (Curiously Recurring Template Pattern)](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
- Related findings: #1 (token parsing), #2 (UI helpers), #3 (wizard state), #4 (wizard completion)
