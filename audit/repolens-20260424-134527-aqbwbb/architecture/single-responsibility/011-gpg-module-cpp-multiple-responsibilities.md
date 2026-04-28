---
title: "[MEDIUM] GpgModule.cpp combines UI logic, serial commands, storage operations, and PIN management"
severity: MEDIUM
domain: mod_gpg
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/mod_gpg/src/GpgModule.cpp` (661 lines) handles multiple distinct responsibilities:
1. **UI logic** - View building, wizard state, menu callbacks, QR code display
2. **Serial commands** - `cmd_gpg_status()`, `cmd_gpg_generate()`, `cmd_gpg_export()`, `cmd_gpg_reset()`
3. **Storage operations** - Direct calls to `GpgStorage::instance()` for CRUD operations
4. **PIN management** - PIN change flow, `PinManager` integration
5. **i18n registration** - All module string translations (English and German)

## Impact
- **High coupling**: UI changes, serial command changes, and storage logic all in same file
- **Complexity**: 661 lines with mixed concerns makes navigation hard
- **Testing difficulty**: Cannot test serial commands without UI initialization
- **Hard to reuse**: Storage logic is tightly coupled to module-specific UI

## Evidence
File: `components/mod_gpg/src/GpgModule.cpp`
- Lines 28-106: i18n string registration (17 strings x 2 languages)
- Lines 107-200: Serial command handlers
- Lines 200-400: UI state and buffer management
- Lines 400-661: UI logic, callbacks, wizard flow

Key pattern showing mixed concerns:
```cpp
// Serial command handler
static void cmd_gpg_generate(const char* args) {
    // Parse args
    char curve_str[8] = {};
    const char* p = nextToken(args, curve_str, sizeof(curve_str));
    // ...
    // Direct storage call
    bool ok = GpgStorage::instance().generateKey(curve, user_id);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

// UI callback with PIN flow
static void onGenerateKey() {
    // Start wizard
    wizardStep = 0;
    showGenerateWizard();
}

// PIN change integration
static void onPinChangeComplete() {
    // Continue GPG flow
    ...
}
```

## Recommended Fix
Split into focused modules:
1. **GpgUi** - UI logic and views in `components/mod_gpg/src/GpgUi.cpp`
2. **GpgCommands** - Serial commands in `components/mod_gpg/src/GpgCommands.cpp`
3. **GpgModule** - Module registration and orchestration in `components/mod_gpg/src/GpgModule.cpp`

The `GpgStorage` class (already separate) should remain the data layer.

Each module should:
- Have its own header file
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/mod_gpg/src/
  GpgModule.cpp  // Module registration, orchestration
  GpgUi.cpp      // UI logic, views, callbacks
  GpgCommands.cpp // Serial commands
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Layered Architecture: https://en.wikipedia.org/wiki/Layered_architecture
