---
title: "[MEDIUM] Serial command handlers mixed with module business logic"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
Serial command handlers are implemented inline within module files, mixing command-line interface concerns with core business logic. The TOTP and Password modules have 100+ lines of command parsing and handler code embedded directly in the module source.

**Location**: 
- `components/mod_totp/src/TotpModule.cpp:96-328` (230 lines of command handlers)
- `components/mod_password/src/PasswordModule.cpp:116-329` (210 lines of command handlers)

## Impact
- **Code bloat**: Module files grow larger with unrelated CLI code
- **Difficult testing**: Command parsing logic mixed with business logic
- **Limited reusability**: Commands tightly coupled to module implementation
- **Maintenance burden**: Changing CLI format affects module code

## Evidence
```cpp
// TotpModule.cpp:96-328 - 230 lines of serial commands
static constexpr const char* CMD_MODULE = "totp";
static bool s_commandsRegistered = false;

static const char* skipSpaces(const char* s) { ... }
static const char* nextToken(const char* s, char* out, size_t outSize) { ... }
static uint8_t parseAlgo(const char* token) { ... }
static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) { ... }

static void cmd_totp_list(const char* args) { ... }
static void cmd_totp_add(const char* args) { ... }
static void cmd_totp_del(const char* args) { ... }
static void cmd_totp_get(const char* args) { ... }

static void registerCommands() {
    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
    // ...
}

// Called from init() - CLI concerns mixed with business init
bool TotpModule::init() {
    registerStrings();
    registerCommands();  // CLI registration in business init
    // ...
}
```

## Recommended Fix
1. **Create dedicated command handler files**:
   ```
   components/mod_totp/
   ├── src/
   │   ├── TotpModule.cpp      (business logic only)
   │   ├── TotpStore.cpp       (data access)
   │   └── TotpCommands.cpp    (serial commands)
   └── include/mod_totp/
       └── TotpCommands.h
   ```

2. **Extract command handlers**:
   ```cpp
   // TotpCommands.h
   namespace cdc::mod_totp {
       void registerTotpCommands();
   }
   
   // TotpCommands.cpp
   static void cmd_totp_add(const char* args) {
       // Pure command parsing
       auto& store = TotpStore::instance();
       bool ok = store.addAccount(...);
       cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
   }
   
   void registerTotpCommands() {
       auto& reg = cdc::serial::getCommandRegistry();
       reg.registerCommand({"TOTP_ADD", "Add TOTP account", cmd_totp_add, "totp", true});
   }
   ```

3. **Module init calls command registration**:
   ```cpp
   bool TotpModule::init() {
       registerStrings();
       mod_totp::registerTotpCommands();  // Clean separation
       // ...
   }
   ```

## References
- [Command Pattern](https://en.wikipedia.org/wiki/Command_pattern)
- [Separation of concerns - Command handling](https://en.wikipedia.org/wiki/Separation_of_concerns)
