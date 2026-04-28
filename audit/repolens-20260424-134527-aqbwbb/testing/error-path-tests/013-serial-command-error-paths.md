---
title: "[MEDIUM] Untested serial command error paths and edge cases"
severity: MEDIUM
domain: serial_cmd
lens: error-path-tests
labels:
  - "audit:testing/error-path-tests"
---

## Summary
The serial command interface in `components/serial_cmd/` has multiple error paths that are never tested:

**Command Registry (`CommandRegistry.cpp`):**
1. **Command registration failure** - When `MAX_COMMANDS` (64) is reached, returns `false` but never tested
2. **Duplicate command detection** - Case-insensitive duplicate check but never tested with duplicates
3. **Null command name handling** - `strcasecmp()` called on potentially null `cmd.name` but never tested
4. **Unknown command handling** - Prints error but behavior not verified in tests
5. **PIN lockout path** - When badge is blocked, only PING allowed - this path never tested
6. **Authentication bypass** - When `FEATURE_SECURE_SERIAL` enabled, commands blocked without auth - never tested

**Serial Command Processor (`SerialCmd.cpp`):**
1. **Command buffer overflow** - `s_cmdBuffer` has `CMD_BUFFER_SIZE` limit but overflow not tested
2. **History buffer wrapping** - Ring buffer logic not tested with full history
3. **Escape sequence parsing** - `EscState` parser state machine never tested with malformed sequences
4. **NVS operations** - NVS read/write failures not tested (lines 646-660, 528-540)
5. **Slot parsing errors** - Invalid slot numbers not tested
6. **Date/time validation** - Year range checks (2020-2100) not tested with invalid dates

**Evidence:**
- `components/serial_cmd/src/CommandRegistry.cpp:38-52` - Command registration with limits
- `components/serial_cmd/src/CommandRegistry.cpp:114-139` - PIN lockout logic
- `components/serial_cmd/src/CommandRegistry.cpp:143-150` - Per-command auth check
- `components/serial_cmd/src/SerialCmd.cpp:528-540` - NVS read error handling
- `components/serial_cmd/src/SerialCmd.cpp:646-660` - NVS commit error handling
- `components/serial_cmd/src/SerialCmd.cpp:1490` - Total file size, complex logic

**Current test coverage:**
- Zero tests for serial command module
- Only 3 trivial smoke tests exist in entire codebase (all for vCard module)
- No integration tests for command parsing flow

## Impact
**Security Risk:**
- PIN lockout logic might have bugs that allow command execution when blocked
- Authentication bypass might not work correctly if edge cases aren't tested
- Command buffer overflow could corrupt stack if boundary not enforced

**Usability Risk:**
- Unknown commands might not provide helpful error messages
- History navigation might break with edge cases (empty history, full history)
- Escape sequence handling might misinterpret special keys

**Maintenance Risk:**
- No regression tests for command parsing logic
- Adding new commands could break existing behavior without detection
- Feature flag changes (`FEATURE_SECURE_SERIAL`) not verified with tests

## Evidence
**Code snippets showing untested error paths:**

```cpp
// CommandRegistry.cpp:38-52
bool registerCommand(const Command& cmd) override {
    if (count_ >= MAX_COMMANDS) {
        LOG_W(TAG, "Command limit reached");
        return false;  // Never tested with 64+ commands
    }
    // Check for duplicate
    for (size_t i = 0; i < count_; i++) {
        if (strcasecmp(commands_[i].name, cmd.name) == 0) {
            LOG_W(TAG, "Command '%s' already registered", cmd.name);
            return false;  // Never tested with duplicate registration
        }
    }
    // ...
}

// CommandRegistry.cpp:114-139
#if FEATURE_SECURE_SERIAL
    if (pm.isBadgeBlocked()) {
        if (strcasecmp(cmdBuf, "PING") != 0) {
            if (pm.isLockoutActive()) {
                Console::printf("ERROR: PIN locked. Wait %lu seconds.\r\n", ...);
            } else {
                Console::printf("ERROR: PIN permanently locked.\r\n");
            }
            return true;  // Command blocked path never tested
        }
    }
#endif

// SerialCmd.cpp:528-540
static void cmd_nvs_get(const char* args) {
    // ...
    nvs_handle_t nvs;
    err = nvs_open(namespace, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        // ...
    } else {
        Console::printf("ERROR: NVS namespace not found\r\n");  // Error path
    }
    // ...
}
```

**Escape sequence parsing (untested state machine):**
```cpp
// SerialCmd.cpp:60-62
enum class EscState : uint8_t { NONE, ESC, BRACKET };
static EscState s_escState = EscState::NONE;
// State transitions never tested with:
// - Incomplete sequences: ESC, ESC[, ESC[1
// - Invalid sequences: ESCX, ESC[A, ESC[1B
// - Rapid sequences: ESC[1;2HESC[1;2H
```

## Recommended Fix
Create comprehensive tests for serial command module:

1. **Command registry tests** (`test/test_serial_cmd_registry/`):
   - Test command registration success/failure
   - Test duplicate command detection
   - Test command limit (64 commands)
   - Test module unregistration
   - Test null/empty command names

2. **PIN lockout tests** (`test/test_serial_lockout/`):
   - Test that commands are blocked when PIN is locked
   - Test that PING is allowed when locked
   - Test lockout timer behavior
   - Test permanent lockout path

3. **Authentication tests** (`test/test_serial_auth/`):
   - Test command blocking when not authenticated
   - Test AUTH command
   - Test authentication timeout
   - Test per-command auth requirements

4. **Command parsing tests** (`test/test_serial_parsing/`):
   - Test unknown command handling
   - Test command buffer overflow protection
   - Test history add/get/wrap
   - Test escape sequence parsing with valid/invalid sequences
   - Test NVS error handling (namespace not found, read/write fail)

5. **Integration tests** (`test/test_serial_integration/`):
   - Test complete command flow
   - Test multiline input with line interceptor
   - Test HELP command output format

Each test file should be ~1 hour of work and follow existing test structure.

## References
- ESP32 NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- ANSI escape codes: https://en.wikipedia.org/wiki/ANSI_escape_code
- Existing test structure: `test/test_vcard_store/test_vcard_store.cpp`
- Feature flags: `components/cdc_core/feature_flags.h`
