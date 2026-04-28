---
title: "[MEDIUM] TotpModule.cpp combines UI logic, serial commands, store operations, and i18n"
severity: MEDIUM
domain: mod_totp
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/mod_totp/src/TotpModule.cpp` (1020 lines) handles multiple distinct responsibilities:
1. **UI logic** - View building, wizard state, menu callbacks, list rendering
2. **Serial commands** - `cmd_totp_list()`, `cmd_totp_add()`, `cmd_totp_get()`, `cmd_totp_del()`
3. **Store operations** - Direct calls to `TotpStore::instance()` for CRUD operations
4. **i18n registration** - All module string translations (English and German)
5. **State management** - Wizard state, list buffers, view instances

## Impact
- **High coupling**: UI changes, serial command changes, and store logic all in same file
- **Complexity**: 1020 lines with mixed concerns makes navigation hard
- **Testing difficulty**: Cannot test serial commands without UI initialization
- **Hard to reuse**: Store logic is tightly coupled to module-specific UI

## Evidence
File: `components/mod_totp/src/TotpModule.cpp`
- Lines 22-93: i18n string registration (14 strings x 2 languages)
- Lines 96-250: Serial command handlers (parse args, call store)
- Lines 252-400: UI state and buffer management
- Lines 400-1020: UI logic, callbacks, wizard flow

Key pattern showing mixed concerns:
```cpp
// Serial command handler
static void cmd_totp_add(const char* args) {
    TotpEntry entry = {};
    // Parse args
    const char* p = nextToken(args, account, sizeof(account));
    // ...
    // Direct store call
    bool ok = TotpStore::instance().addEntry(entry);
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

// i18n registration
i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::EN, "TOTP");
i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::DE, "TOTP");
```

## Recommended Fix
Split into focused modules:
1. **TotpUi** - UI logic and views in `components/mod_totp/src/TotpUi.cpp`
2. **TotpCommands** - Serial commands in `components/mod_totp/src/TotpCommands.cpp`
3. **TotpModule** - Module registration and orchestration in `components/mod_totp/src/TotpModule.cpp`

The `TotpStore` class (already separate) should remain the data layer.

Each module should:
- Have its own header file
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/mod_totp/src/
  TotpModule.cpp  // Module registration, orchestration
  TotpUi.cpp      // UI logic, views, callbacks
  TotpCommands.cpp // Serial commands
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Layered Architecture: https://en.wikipedia.org/wiki/Layered_architecture
