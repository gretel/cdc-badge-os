---
title: "[MEDIUM] PasswordModule.cpp combines UI logic, serial commands, and store operations"
severity: MEDIUM
domain: mod_password
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/mod_password/src/PasswordModule.cpp` (856 lines) handles multiple distinct responsibilities:
1. **UI logic** - View building, wizard state, menu callbacks
2. **Serial commands** - `cmd_password_list()`, `cmd_password_get()`, `cmd_password_add()`, `cmd_password_del()`
3. **Store operations** - Direct calls to `PasswordStore::instance()` for CRUD operations
4. **i18n registration** - All module string translations
5. **State management** - Wizard state, list buffers, view instances

## Impact
- **High coupling**: UI changes, serial command changes, and store logic all in same file
- **Complexity**: 856 lines with mixed concerns makes navigation hard
- **Testing difficulty**: Cannot test serial commands without UI initialization
- **Hard to reuse**: Store logic is tightly coupled to module-specific UI

## Evidence
File: `components/mod_password/src/PasswordModule.cpp`
- Lines 23-117: i18n string registration
- Lines 121-332: Serial command handlers
- Lines 334-400: UI state and buffer management
- Lines 400-856: UI logic, callbacks, wizard

Key pattern showing mixed concerns:
```cpp
// Serial command handler
static void cmd_password_add(const char* args) {
    PasswordEntry entry = {};
    // Parse args
    const char* p = nextToken(args, title, sizeof(title));
    // ...
    // Direct store call
    bool ok = PasswordStore::instance().addEntry(entry);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

// UI callback
static void onActionsSelect(uint16_t index) {
    switch (index) {
        case 0: onView(); break;
        case 1: onEdit(); break;
        case 2: onDelete(); break;
    }
}
```

## Recommended Fix
Split into focused modules:
1. **PasswordUi** - UI logic and views in `components/mod_password/src/PasswordUi.cpp`
2. **PasswordCommands** - Serial commands in `components/mod_password/src/PasswordCommands.cpp`
3. **PasswordModule** - Module registration and orchestration in `components/mod_password/src/PasswordModule.cpp`

The `PasswordStore` class (already separate) should remain the data layer.

Each module should:
- Have its own header file
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/mod_password/src/
  PasswordModule.cpp  // Module registration, orchestration
  PasswordUi.cpp      // UI logic, views, callbacks
  PasswordCommands.cpp // Serial commands
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Layered Architecture: https://en.wikipedia.org/wiki/Layered_architecture
