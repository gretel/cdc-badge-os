---
title: "[LOW] Command registration uses global static flags creating module coupling"
severity: LOW
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
Multiple modules use global static boolean flags to track command registration state. This creates coupling because the registration functions check module-level state instead of instance state, making it harder to reason about module lifecycle and potentially causing issues with module re-initialization.

**Evidence locations:**
- `components/mod_gpg/src/GpgModule.cpp:108` - `s_commandsRegistered`
- `components/mod_password/src/PasswordModule.cpp:108` - `s_commandsRegistered`
- `components/mod_totp/src/TotpModule.cpp:47` - `s_commandsRegistered`

## Impact
- **Module lifecycle unclear**: Commands registered in global scope, not tied to module instance
- **Hard to reset**: If module needs re-initialization, command registration state is unclear
- **Testing difficulty**: Tests need to reset global flags between runs

## Evidence
```cpp
// components/mod_gpg/src/GpgModule.cpp:108
static constexpr const char* CMD_MODULE = "gpg";
static bool s_commandsRegistered = false;

// Line 116-126
static void registerCommands() {
    if (s_commandsRegistered) return;  // Global state check
    s_commandsRegistered = true;
    auto& registry = cdc::serial::getCommandRegistry();
    registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
    registry.registerCommand({"GPG_GENERATE", "Generate GPG keys", cmd_gpg_generate, CMD_MODULE, true});
    registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});
    registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
}
```

```cpp
// components/mod_password/src/PasswordModule.cpp:108
static constexpr const char* CMD_MODULE = "password";
static bool s_commandsRegistered = false;

// Line 326-335
static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;

    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
    // ...
}
```

## Recommended Fix
**Use module instance state instead of global flags:**

1. **Add command registration state to module:**
```cpp
// components/mod_gpg/include/mod_gpg/GpgModule.h
class GpgModule : public core::IModule {
public:
    // ...
private:
    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    core::IModule::SlotRange slotRange_ = {};
    bool commandsRegistered_ = false;  // Instance state
};

// components/mod_gpg/src/GpgModule.cpp
static void registerCommands(GpgModule& module) {
    if (module.commandsRegistered_) return;
    module.commandsRegistered_ = true;
    // ...
}

bool GpgModule::init() {
    registerCommands(*this);  // Pass instance
    // ...
}
```

2. **Or use command registry with module-scoped registration:**
```cpp
// In registerCommands():
auto& registry = cdc::serial::getCommandRegistry();
registry.registerCommand({"GPG_STATUS", ...}, getName());  // Already done
// No need for s_commandsRegistered flag - registry tracks it
```

## References
- [Encapsulation on Wikipedia](https://en.wikipedia.org/wiki/Encapsulation_(computer_programming))
