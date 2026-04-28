---
title: "[LOW] Escape key in serial console only parses sequences, doesn't cancel input"
severity: LOW
domain: interaction-design
lens: keyboard-navigation
labels:
  - escape-key
  - serial-console
  - keyboard-shortcuts
---

## Summary

The serial command interface parses Escape sequences for arrow keys but doesn't provide a way to use Escape to cancel the current input line. Users must use Ctrl+C or Ctrl+U to clear the line, but Escape (a common "cancel" key) does nothing.

**Evidence locations:**
- `components/serial_cmd/src/SerialCmd.cpp` - Escape handling (lines 1269-1272)
- `components/serial_cmd/src/SerialCmd.cpp` - Special key handling (lines 1274-1310)

The Escape key is only used as a prefix for arrow key sequences, not as a standalone cancel action.

## Impact

**User Experience Impact:**
- Users familiar with terminal conventions expect Escape to cancel input
- Inconsistent with common terminal behavior (Escape often clears line or cancels)
- Users may accidentally press Escape and wonder why nothing happens

**Consistency Impact:**
- Other UI components use N as "cancel", but serial console doesn't follow this pattern
- Missing a standard keyboard shortcut for line cancellation

## Evidence

**File: `components/serial_cmd/src/SerialCmd.cpp` (lines 1269-1272)**
```cpp
case 0x1B:  // ESC
    s_escState = EscState::ESC;
    return false;
```

Escape is only used to start an escape sequence parser. If no key follows, the state is reset and nothing happens.

**File: `components/serial_cmd/src/SerialCmd.cpp` (lines 1274-1310)**
```cpp
// Handle special characters
switch (c) {
    case 0x1B:  // ESC
        s_escState = EscState::ESC;
        return false;

    case '\r':
    case '\n':
        // ...
        return true;

    case 0x7F:  // Backspace (DEL)
    case 0x08:  // Backspace (BS)
        // ...
        return false;

    case 0x03:  // Ctrl+C
        Console::print("^C\r\n");
        s_cmdBufferPos = 0;
        s_historyPos = 0;
        Console::showPrompt();
        return false;

    case 0x15:  // Ctrl+U
        while (s_cmdBufferPos > 0) {
            Console::print("\b \b");
            s_cmdBufferPos--;
        }
        return false;

    default:
        // ...
}
```

Escape is handled separately from the special character switch, and doesn't trigger any cancellation.

## Recommended Fix

1. **Add Escape to cancel current input line**:
   ```cpp
   case 0x1B:  // ESC - Cancel current input
       if (s_cmdBufferPos > 0) {
           // Clear the line
           while (s_cmdBufferPos > 0) {
               Console::print("\b \b");
               s_cmdBufferPos--;
           }
           Console::print("\r\n");
           Console::showPrompt();
       } else {
           // Just reset escape state if line is empty
           s_escState = EscState::NONE;
       }
       s_historyPos = 0;
       return false;
   ```

2. **Or, add Escape to start of special character switch** for cleaner code:
   ```cpp
   switch (c) {
       case 0x1B:  // ESC
           if (s_escState == EscState::BRACKET) {
               // Handle arrow keys (existing logic)
           } else {
               // Cancel current line
               // ...
           }
           break;
       // ...
   }
   ```

3. **Document the Escape behavior** in the console help:
   ```
   ESC = Cancel current line
   Ctrl+C = Cancel and show prompt
   Ctrl+U = Clear line
   ```

## References

- GNU Readline conventions: Escape often clears current word or line
- Common terminal behavior for Escape key
- ESP32-S3 CDC Badge serial console documentation
