---
title: "[MEDIUM] No Deprecation Mechanism for Serial Commands"
severity: MEDIUM
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The serial command interface has no way to mark commands as deprecated. When commands are replaced or removed, existing clients receive generic "unknown command" errors without hints to use alternatives.

**Evidence:**
- `components/serial_cmd/src/CommandRegistry.cpp:162-165` - Unknown command shows generic error
- `docs/SERIAL_COMMANDS.md` - No deprecated commands section or sunset dates

## Impact
- Clients using old commands get confusing error messages
- No grace period for transitioning to new commands
- Hard to evolve the API without breaking existing integrations

## Evidence
Current error handling:
```cpp
Console::printf("ERROR: Unknown command '%s'\r\n", cmdBuf);
Console::printf("Type 'HELP' for available commands.\r\n");
```

No deprecation warnings, no alternative suggestions.

## Recommended Fix
Add deprecation support:

1. Add deprecation field to Command struct:
```cpp
struct Command {
    const char* name;
    const char* help;
    CommandHandler handler;
    const char* moduleName;
    bool requiresAuth;
    const char* deprecatedSince;     // Version when deprecated (nullptr if not)
    const char* alternative;         // Replacement command (nullptr if none)
};
```

2. Update command processor:
```cpp
// In registerCommand():
if (cmd.deprecatedSince && cmd.alternative) {
    Console::printf("WARNING: '%s' deprecated since v%s, use '%s'\r\n",
                   cmd.name, cmd.deprecatedSince, cmd.alternative);
}
```

3. Add to HELP output:
```cpp
// In showHelp():
if (commands_[i].deprecatedSince) {
    Console::printf("  %-20s %s (DEPRECATED: use %s)\r\n",
                   commands_[i].name,
                   commands_[i].help,
                   commands_[i].alternative);
}
```

## References
- API Deprecation Best Practices: https://cloud.google.com/apis/design/versioning#deprecation
- HTTP Deprecation Header: https://tools.ietf.org/html/draft-ietf-httpbis-deprecation-header-01
