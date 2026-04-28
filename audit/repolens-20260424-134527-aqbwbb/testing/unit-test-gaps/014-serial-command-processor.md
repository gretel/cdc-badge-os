---
title: "[HIGH] Serial command processor lacks unit test coverage"
severity: HIGH
domain: serial_cmd
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `serial_cmd` component in `components/serial_cmd/` provides the complete serial console command interface with 30+ command handlers, line editing, history, and authentication. However, **no unit tests exist** for this critical component. The existing test files only cover vCard module linking, not serial command processing.

**Untested Public Functions** (from `SerialCmd.h` and `SerialCmd.cpp`):

**Core API** (`components/serial_cmd/include/serial_cmd/SerialCmd.h`):
- `SerialCmd::init()` - Initialize console and register commands
- `SerialCmd::process()` - Process one input character, return true if command completed
- `SerialCmd::getRegistry()` - Get command registry
- `SerialCmd::setTextCallback()` - Set text change callback
- `SerialCmd::setTimeCallback()` - Set time change callback
- `SerialCmd::isAuthenticated()` - Check session auth status
- `SerialCmd::authenticate(const char* pin)` - Authenticate with PIN
- `SerialCmd::logout()` - End authenticated session
- `SerialCmd::executeCommand(char* cmd)` - Execute command line
- `SerialCmd::trim(char* str)` - Trim whitespace
- `SerialCmd::registerBuiltinCommands()` - Register 30+ built-in commands

**Command Registry** (`ICommandRegistry.h`):
- `ICommandRegistry::registerCommand()` - Register a command
- `ICommandRegistry::unregisterModule()` - Unregister module commands
- `ICommandRegistry::processCommand(const char* line)` - Process command line
- `ICommandRegistry::showHelp()` - Display help
- `ICommandRegistry::getCommandCount()` - Get command count
- `ICommandRegistry::setAuthProvider()` - Set authentication callback
- `ICommandRegistry::setOnCommandExecuted()` - Set execution callback

**Console I/O** (`Console.h`):
- `Console::init()` - Initialize console
- `Console::printf()` - Formatted output
- `Console::print()` - String output
- `Console::putchar()` - Single character
- `Console::getchar()` - Non-blocking input
- `Console::flush()` - Flush output
- `Console::available()` - Check input available
- `Console::showPrompt()` - Show prompt

## Impact
**Operational Risk**: The serial console is the primary debugging and administration interface. Without tests:
- Command parsing edge cases may fail (whitespace, special chars)
- History navigation (arrow keys) may have off-by-one errors
- Authentication timeout may not work correctly
- NVS commands could corrupt data without validation
- TR01 wipe command lacks confirmation flow tests
- No regression tests for command additions/changes

**Security Risk**: The `authenticate()` function handles PIN verification. Without tests:
- Lockout logic may not work correctly
- Timeout calculation may have bugs
- Empty PIN handling may be incomplete

**Maintenance Cost**: 1490 lines of command processing code with zero tests makes refactoring risky.

## Evidence
**File**: `components/serial_cmd/include/serial_cmd/SerialCmd.h`
```cpp
class SerialCmd {
public:
    static constexpr size_t CMD_BUFFER_SIZE = 128;
    static constexpr uint32_t AUTH_TIMEOUT_MS = 30000;
    
    static void init();
    static bool process();
    static ICommandRegistry& getRegistry();
    static void setTextCallback(TextChangeCallback callback);
    static void setTimeCallback(TimeChangeCallback callback);
    static bool isAuthenticated();
    static bool authenticate(const char* pin);
    static void logout();
    static void executeCommand(char* cmd);
    static char* trim(char* str);
    static void registerBuiltinCommands();
};
```

**File**: `components/serial_cmd/src/SerialCmd.cpp` - Key untested functions:
- Lines 95-110: `historyAdd()` - History ring buffer
- Lines 112-118: `historyGet()` - History retrieval
- Lines 120-135: `redrawLine()` - Line editing
- Lines 160-185: `parseSlotArg()` - Slot parsing
- Lines 400-410: `cmdHelp()` - Help command
- Lines 470-480: `cmdNvsClear()` - NVS erase with confirmation
- Lines 805-855: `cmdAuth()` - Authentication with lockout
- Lines 1135-1189: `cmdTr01Wipe()` - Factory wipe with progress

**Existing Tests**: Only 3 smoke tests exist, none test serial commands:
- `test_vcard_module_link.cpp`
- `test_vcard_store.cpp`
- `test_ble_vcard_symbols.cpp`

## Recommended Fix
Create test file `test/test_serial_cmd/test_serial_cmd.cpp` with test cases for:

1. **Command parsing tests**:
   - Test `trim()` removes leading/trailing whitespace
   - Test `trim()` handles empty strings
   - Test `trim()` handles single word
   - Test `executeCommand()` with valid command
   - Test `executeCommand()` with empty command (no-op)
   - Test `executeCommand()` with unknown command

2. **History tests**:
   - Test historyAdd() stores commands
   - Test historyGet() retrieves in reverse order
   - Test history ring buffer wraps correctly
   - Test history limit (HISTORY_MAX = 10)

3. **Authentication tests**:
   - Test `authenticate()` with correct PIN
   - Test `authenticate()` with wrong PIN
   - Test `authenticate()` with empty PIN
   - Test `isAuthenticated()` returns true after auth
   - Test `isAuthenticated()` returns false after logout
   - Test authentication timeout (FEATURE_SECURE_SERIAL)
   - Test lockout after max retries

4. **Console I/O tests**:
   - Test `process()` handles backspace
   - Test `process()` handles Enter/CR/LF
   - Test `process()` handles arrow up (history)
   - Test `process()` handles arrow down (history)
   - Test `process()` handles Ctrl+C (clear line)
   - Test `process()` handles Ctrl+U (clear line)
   - Test `process()` handles ESC sequences

5. **Command handler tests**:
   - Test `cmdHelp()` shows command list
   - Test `cmdPing()` returns PONG
   - Test `cmdStatus()` shows heap/uptime
   - Test `cmdMem()` shows memory stats
   - Test `cmdNvsList()` with empty NVS
   - Test `cmdNvsRead()` with valid key
   - Test `cmdNvsRead()` with missing key
   - Test `cmdTr01Status()` shows session state
   - Test slot parsing with valid/invalid values

6. **Callback tests**:
   - Test `setTextCallback()` is called on SET_NAME
   - Test `setTimeCallback()` is called on SET_TIME/SET_DATE

**Estimated effort**: ~1 hour for core parsing/auth tests. Additional hour for command handlers.

## References
- Similar test patterns: `test/test_vcard_store/test_vcard_store.cpp`
- Command registry interface: `components/serial_cmd/include/serial_cmd/ICommandRegistry.h`
- Console interface: `components/serial_cmd/include/serial_cmd/Console.h`
