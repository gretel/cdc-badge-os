---
title: "[LOW] Serial command registration runs at startup"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
Serial commands are registered **synchronously** during module initialization. Each module registers its commands in a loop, adding to startup time even though commands may never be used.

**Location:** `components/mod_totp/src/TotpModule.cpp:326-328`, similar patterns in other modules

## Evidence
From `components/mod_totp/src/TotpModule.cpp:326-328`:
```cpp
static void registerCommands() {
    if (s_commandsRegistered) return;
    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
    reg.registerCommand({"TOTP_ADD", "Add TOTP account", cmd_tota_add, CMD_MODULE, true});
    reg.registerCommand({"TOTP_DEL", "Delete TOTP account by index", cmd_totp_del, CMD_MODULE, true});
    reg.registerCommand({"TOTP_GET", "Generate TOTP code by index", cmd_totp_get, CMD_MODULE, true});
    s_commandsRegistered = true;
}
```

Called from `init()`:
```cpp
bool TotpModule::init() {
    registerStrings();
    registerCommands();  // 4 function calls
    // ...
}
```

With 10 modules averaging 4 commands each:
- 40 `registerCommand()` calls
- Each call: string copy + table insertion + sorting
- Estimated: 40 × 0.1ms = 4ms total

## Recommended Fix
**Option 1**: Batch registration with static tables
```cpp
static const cdc::serial::Command s_totpCommands[] = {
    {"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true},
    {"TOTP_ADD", "Add TOTP account", cmd_totp_add, CMD_MODULE, true},
    // ...
};

static void registerCommands() {
    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommands(s_totpCommands, sizeof(s_totpCommands)/sizeof(s_totpCommands[0]));
}
```

**Option 2**: Lazy command registration
```cpp
static bool s_commandsRegistered = false;
static void ensureCommandsRegistered() {
    if (!s_commandsRegistered) {
        registerCommands();
        s_commandsRegistered = true;
    }
}

// Call from serial command handler or on first access
```

## References
- Serial commands: [Command registry](components/serial_cmd/CommandRegistry.h)
