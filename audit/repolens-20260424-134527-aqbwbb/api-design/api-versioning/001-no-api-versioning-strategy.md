---
title: "[MEDIUM] No API Versioning Strategy for Serial Command Interface"
severity: MEDIUM
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The serial command interface (`components/serial_cmd/`) has no versioning mechanism. Commands are registered dynamically via `CommandRegistry` with no version tracking or backward compatibility guarantees.

**Evidence:**
- `components/serial_cmd/include/serial_cmd/ICommandRegistry.h:17-23` - Command struct has no version field
- `components/serial_cmd/src/CommandRegistry.cpp:39-58` - `registerCommand()` accepts commands without version info
- `docs/SERIAL_COMMANDS.md` - Command reference has no version information or deprecation notices

## Impact
When command signatures change (e.g., `TOTP_ADD` parameters, new required fields), existing clients/scripts will break without warning. No migration path exists for consumers of the serial API.

## Evidence
Current command registration structure:
```cpp
struct Command {
    const char* name;           // Command name (e.g., "TOTP_LIST")
    const char* help;           // Help text
    CommandHandler handler;     // Handler function
    const char* moduleName;     // Module that registered this command
    bool requiresAuth;          // Requires authentication
};
```

No version field, no deprecation marker, no compatibility layer.

## Recommended Fix
Add version tracking to the serial command API:

1. Add version field to `Command` struct:
```cpp
struct Command {
    const char* name;
    const char* help;
    CommandHandler handler;
    const char* moduleName;
    bool requiresAuth;
    uint8_t minVersion;         // Minimum API version required
    uint8_t maxVersion;         // Maximum supported version (0 = current)
};
```

2. Add version query command:
```cpp
// Add to COMMANDS array in SerialCmd.cpp
{ "API_VERSION", "Get API version", cmdApiVersion, "system", false }
```

3. Document version in `docs/SERIAL_COMMANDS.md` with changelog section

## References
- Semantic Versioning: https://semver.org/
- API Versioning Best Practices: https://www.rapidapi.com/blog/api-versioning-best-practices/
