---
title: "[MEDIUM] Serial command interface uses global state creating coupling"
severity: MEDIUM
domain: architecture/coupling
lens: serial-cmd-coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
The `serial_cmd` component uses global static functions and state (`SerialCmd::init()`, `SerialCmd::process()`, `CommandRegistry::instance()`). Modules register commands directly with the registry, creating implicit coupling to the serial subsystem.

**Evidence:**
- `components/serial_cmd/src/CommandRegistry.cpp` - Singleton registry
- `components/mod_gpg/src/GpgModule.cpp` - Registers serial commands
- `components/mod_totp/src/TotpModule.cpp` - Registers serial commands
- `main/main.cpp` (line 219): Calls `SerialCmd::init()` and `SerialCmd::process()`

## Impact
**Global state:** Command registry is a global singleton. Any module can register commands at any time.

**Hidden coupling:** Modules depend on serial commands being available. If serial is disabled, modules still register commands.

**Testing complexity:** To test a module, you need the serial subsystem initialized.

**Example:**
```cpp
// mod_gpg/src/GpgModule.cpp
static void registerCommands() {
    CommandRegistry::instance().registerCommand(
        "gpg.list",
        [](const char* args) { ... }
    );
}

// Called from anywhere, creates hidden dependency on serial
```

## Evidence
**File: `components/serial_cmd/src/CommandRegistry.cpp` (lines 1-30)**
```cpp
class CommandRegistry {
public:
    static CommandRegistry& instance() {
        static CommandRegistry instance;
        return instance;
    }
    
    void registerCommand(const char* name, CommandHandler handler) {
        // ...
    }
};
```

**File: `main/main.cpp` (lines 219-222)**
```cpp
// Initialize Serial Command Interface
cdc::serial::SerialCmd::init();
LOG_I(TAG, "Serial Command Interface ready");
```

**File: `main/main.cpp` (lines 242-243)**
```cpp
while (true) {
    EventBus::instance().process();
    cdc::serial::SerialCmd::process();  // Called every loop
    ...
}
```

**File: `components/mod_gpg/src/GpgModule.cpp` (lines 655-658)**
```cpp
static void registerCommands() {
    cdc::serial::CommandRegistry::instance().registerCommand(
        "gpg.list", [](const char* args) { ... }
    );
}
```

## Recommended Fix
**Option 1: Event-based commands**
Modules emit "register command" events. Serial subsystem listens and registers.

**Option 2: Command registry as service**
Register command registry with ServiceRegistry. Modules request it:
```cpp
auto* cmdReg = ServiceRegistry::instance().get<CommandRegistry>("commands");
```

**Option 3: Deferred registration**
Commands registered during module initialization, not on first use.

## References
- `components/serial_cmd/src/CommandRegistry.cpp` - Command registry
- `components/serial_cmd/include/serial_cmd/SerialCmd.h` - Serial command interface
- `main/main.cpp` - Serial initialization
- `components/cdc_core/include/cdc_core/ServiceRegistry.h` - Service registry
