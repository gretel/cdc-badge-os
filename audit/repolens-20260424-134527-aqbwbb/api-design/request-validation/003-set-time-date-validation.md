---
title: "[MEDIUM] SET_TIME and SET_DATE commands lack rigorous input validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "time"
---

## Summary

The `SET_TIME` and `SET_DATE` commands in `components/serial_cmd/src/SerialCmd.cpp` use `sscanf` to parse time and date values but do not validate the complete format of the input string. Trailing characters beyond the expected format are silently ignored, allowing inputs like `"25:30:45 EXTRA"` to be accepted as `"25:30:45"`.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:698-720` (SET_TIME), `732-754` (SET_DATE)

## Impact

- **Silent data loss**: Extra characters in the input are ignored, potentially masking user errors
- **Inconsistent UX**: User might think they set "12:30:45 PM" but only "12:30:45" is set
- **Format ambiguity**: Accepts single-digit hours/minutes without leading zeros (e.g., "9:5:3" instead of "09:05:03")

## Evidence

```cpp
// File: components/serial_cmd/src/SerialCmd.cpp:698-720
static void cmdSetTime(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: SET_TIME HH:MM:SS\r\n");
        return;
    }
    int h, m, s;
    if (sscanf(args, "%d:%d:%d", &h, &m, &s) != 3) {  // Line 705
        Console::printf("ERROR: Invalid format. Use HH:MM:SS\r\n");
        return;
    }
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {  // Line 708
        Console::printf("ERROR: Invalid time values\r\n");
        return;
    }
    // ... sets time ...
}
```

Issues:
1. `sscanf(args, "%d:%d:%d", &h, &m, &s)` accepts `"12:30:45 extra text"` - the extra text is ignored
2. No validation that the entire input string was consumed
3. No validation for leading zeros in format (user might expect strict HH:MM:SS)
4. Accepts negative values initially, then validates range (two-step validation)

Similar issue in `cmdSetDate` at line 732-754 with `sscanf(args, "%d.%d.%d", &d, &m, &y)`.

## Recommended Fix

Add strict format validation by checking for trailing characters:

```cpp
static void cmdSetTime(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: SET_TIME HH:MM:SS\r\n");
        return;
    }
    
    int h, m, s;
    char trailing;
    // Parse with trailing character check
    int parsed = sscanf(args, "%d:%d:%d %c", &h, &m, &s, &trailing);
    
    if (parsed == 4 && !isspace(trailing)) {
        // Extra non-whitespace characters found
        Console::printf("ERROR: Invalid format. Use HH:MM:SS (no extra characters)\r\n");
        return;
    }
    
    if (parsed < 3) {
        Console::printf("ERROR: Invalid format. Use HH:MM:SS\r\n");
        return;
    }
    
    // Validate ranges
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        Console::printf("ERROR: Invalid time values (HH: 0-23, MM: 0-59, SS: 0-59)\r\n");
        return;
    }
    
    // ... rest of function ...
}
```

For stricter format enforcement (require leading zeros):

```cpp
static void cmdSetTime(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: SET_TIME HH:MM:SS\r\n");
        return;
    }
    
    // Check format: exactly 8 chars, positions 2 and 5 are colons
    size_t len = strlen(args);
    if (len != 8 || args[2] != ':' || args[5] != ':') {
        Console::printf("ERROR: Use HH:MM:SS format (exactly 8 characters)\r\n");
        return;
    }
    
    // Check all other chars are digits
    for (int i = 0; i < 8; i++) {
        if (i != 2 && i != 5 && !isdigit(args[i])) {
            Console::printf("ERROR: Hours, minutes, seconds must be digits\r\n");
            return;
        }
    }
    
    int h = (args[0] - '0') * 10 + (args[1] - '0');
    int m = (args[3] - '0') * 10 + (args[4] - '0');
    int s = (args[6] - '0') * 10 + (args[7] - '0');
    
    if (h > 23 || m > 59 || s > 59) {
        Console::printf("ERROR: Invalid time values\r\n");
        return;
    }
    
    // ... rest of function ...
}
```

## References

- CWE-20: Improper Input Validation
- CWE-131: Incorrect calculation of buffer size
- ISO 8601 time format standard
