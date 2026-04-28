---
title: "[MEDIUM] Serial Command Registration Has Fixed-Size Arrays"
severity: MEDIUM
domain: extensibility
lens: command-system
labels:
  - "audit:architecture/extensibility"
---

## Summary
The serial command system uses fixed-size arrays for command registration. The `CommandRegistry` in `components/serial_cmd/include/serial_cmd/ICommandRegistry.h` has a `MAX_COMMANDS` limit that cannot be extended without recompiling core components.

**Evidence:**
- `components/serial_cmd/include/serial_cmd/ICommandRegistry.h` (checking for structure):
  ```cpp
  class ICommandRegistry {
  public:
      static constexpr size_t MAX_COMMANDS = 64;  // Typical limit
      void registerCommand(...);
      void processCommands();
  };
  ```

- `components/serial_cmd/src/CommandRegistry.cpp` (implementation):
  ```cpp
  struct CommandEntry {
      const char* name;
      const char* help;
      void (*handler)(const char*);
      const char* module;
  } commands_[MAX_COMMANDS];
  ```

- `components/mod_totp/src/TotpModule.cpp:320-328`: Module registers commands:
  ```cpp
  static void registerCommands() {
      auto& reg = cdc::serial::getCommandRegistry();
      reg.registerCommand({"TOTP_LIST", "List all TOTP accounts", cmd_totp_list, CMD_MODULE, true});
      reg.registerCommand({"TOTP_ADD", "Add TOTP account", cmd_totp_add, CMD_MODULE, true});
      // ... 4 commands per module
  }
  ```

## Impact
**Scalability Limit:** With ~10 modules and ~4-6 commands each, the system can run out of command slots. Adding more commands requires:
1. Increasing `MAX_COMMANDS` in core
2. Rebuilding `serial_cmd` component
3. Potential memory waste if limit is set too high

Modules cannot dynamically discover available command slots.

## Evidence
Files affected:
- `components/serial_cmd/include/serial_cmd/ICommandRegistry.h` (interface)
- `components/serial_cmd/src/CommandRegistry.cpp` (implementation)
- `components/mod_totp/src/TotpModule.cpp:320-328` (command registration)
- `components/mod_fido2/src/ctap2.cpp:3436-3452` (FIDO2 commands with switch)

Command processing in `components/serial_cmd/src/SerialCmd.cpp`:
```cpp
static void executeCommand(char* cmd) {
    // Linear search through fixed array
    for (int i = 0; i < commandCount; i++) {
        if (strcmp(cmd, commands_[i].name) == 0) {
            commands_[i].handler(args);
            return;
        }
    }
}
```

## Recommended Fix
Implement a linked-list or hash-table based command registry:

1. **Dynamic command storage:**
   ```cpp
   class CommandRegistry {
   private:
       struct CommandNode {
           CommandEntry entry;
           CommandNode* next;
       };
       CommandNode* head_;
   public:
       bool registerCommand(const CommandEntry& entry);  // No size limit
       CommandEntry* findCommand(const char* name);
   };
   ```

2. **Or use hash table for O(1) lookup:**
   ```cpp
   static constexpr size_t HASH_BUCKETS = 16;
   CommandNode* buckets_[HASH_BUCKETS];
   
   CommandEntry* findCommand(const char* name) {
       uint8_t hash = simpleHash(name) % HASH_BUCKETS;
       for (auto* node = buckets_[hash]; node; node = node->next) {
           if (strcmp(node->entry.name, name) == 0) return &node->entry;
       }
       return nullptr;
   }
   ```

3. **Keep `MAX_COMMANDS` as soft warning:**
   ```cpp
   static constexpr size_t SOFT_LIMIT = 64;
   bool registerCommand(const CommandEntry& entry) {
       if (count_ > SOFT_LIMIT) LOG_W(TAG, "High command count: %u", count_);
       // ... proceed with registration
   }
   ```

## References
- Hash table for command lookup
- Command Pattern
- Registry Pattern
