---
title: "[HIGH] Serial command interface lacks integration tests for command parsing and execution"
severity: HIGH
domain: interface
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:serial_cmd"
  - "area:commands"
---

## Summary
The `SerialCmd` component provides a command-line interface for debugging and configuration (e.g., `nvs get`, `tropic info`, `gpg generate`), but **no integration tests** verify that commands are parsed correctly and execute end-to-end.

## Evidence

**SerialCmd API** (`components/serial_cmd/include/serial_cmd/SerialCmd.h`):
```cpp
class SerialCmd {
    static void init();
    static void process();
    static void print(const char* text);
};
```

**CommandRegistry API** (`components/serial_cmd/include/serial_cmd/ICommandRegistry.h`):
```cpp
class ICommandRegistry {
    void registerCommand(const char* name, CommandHandler handler);
    void execute(const char* line);
};
```

**Available commands** (from `components/serial_cmd/src/SerialCmd.cpp`):
- `nvs get <namespace> <key>` - Read NVS
- `nvs set <namespace> <key> <value>` - Write NVS
- `tropic info` - Show TROPIC01 info
- `gpg <subcommand>` - GPG operations
- `fido <subcommand>` - FIDO2 operations
- `totp <subcommand>` - TOTP operations
- `password <subcommand>` - Password operations

**Module registration** (`components/mod_gpg/src/GpgModule.cpp:100+`):
```cpp
static void registerCommands() {
    auto& registry = cdc::serial::CommandRegistry::instance();
    registry.registerCommand("gpg", cmd_gpg);
}
```

**Current test coverage**: None

## Impact
- **Command parsing bugs**: Edge cases (quotes, spaces, special chars) not tested
- **Error handling**: Invalid commands may crash or hang
- **Module commands**: Each module's serial commands untested
- **Authentication**: `nvsedit` commands require auth but not verified

## Recommended Fix

Create integration test `test_serial_commands/` that verifies:

1. **Command parsing**: Arguments split correctly, quotes handled
2. **Built-in commands**: `nvs get/set`, `help`, `clear` work
3. **Module commands**: Each module's commands execute correctly
4. **Error handling**: Invalid commands return error messages
5. **Auth flow**: Locked commands require authentication

**Test structure** (example):
```cpp
// test/test_serial_commands/test_command_parsing.cpp
#include "serial_cmd/SerialCmd.h"
#include "serial_cmd/CommandRegistry.h"

void test_command_execution() {
    SerialCmd::init();
    
    // Test NVS read
    std::string output;
    CommandRegistry::instance().execute("nvs get nvs_config test_key");
    // Verify output captured correctly
}

void test_module_command_gpg() {
    CommandRegistry::instance().execute("gpg status");
    // Verify GPG status command executes
}
```

## References
- [SerialCmd implementation](components/serial_cmd/src/SerialCmd.cpp)
- [CommandRegistry](components/serial_cmd/src/CommandRegistry.cpp)
- [Console output](components/serial_cmd/src/Console.cpp)
