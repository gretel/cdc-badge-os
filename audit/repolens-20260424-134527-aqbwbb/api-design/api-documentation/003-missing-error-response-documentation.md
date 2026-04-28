---
title: "[LOW] Serial command error responses not documented"
severity: LOW
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `docs/SERIAL_COMMANDS.md` documentation lists commands but does not document the error responses that each command can return. Users cannot programmatically handle failures without trial-and-error.

**Missing documentation:**
- Authentication error format and retry behavior
- Invalid parameter error messages
- Slot not found / out of range errors
- Memory allocation failures
- Secure element communication errors

## Impact
- Scripts/programs using the serial interface must parse unstructured error output
- No clear contract for what errors to expect per command
- Difficult to implement robust error handling in client code
- Developers must read source code to understand error conditions

## Evidence
**Example error outputs from code (components/serial_cmd/src/SerialCmd.cpp):**

Authentication errors:
```cpp
Console::printf("ERROR: Not authenticated. Use AUTH <pin> to login.\r\n");
Console::printf("ERROR: Wrong PIN. %d retries remaining.\r\n", retries);
Console::printf("ERROR: PIN locked. Wait %lu seconds.\r\n", remainingSec);
```

Invalid parameter errors:
```cpp
Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);
Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot - 1);
Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
```

Secure element errors:
```cpp
Console::printf("ERROR: Secure Element not available\r\n");
Console::printf("ERROR: Delete failed\r\n");
Console::printf("ERROR: Erase failed\r\n");
Console::printf("ERROR: Session start failed\r\n");
```

**Current documentation (docs/SERIAL_COMMANDS.md):**
Only shows success examples:
```bash
# Add TOTP account
echo "TOTP_ADD GitHub JBSWY3DPEHPK3PXP" > /dev/ttyACM0
```

No error examples or error codes listed.

## Recommended Fix
Add an "Error Handling" section to `docs/SERIAL_COMMANDS.md`:

```markdown
## Error Handling

All errors are prefixed with `ERROR:`. Common error patterns:

### Authentication Errors
- `ERROR: Not authenticated. Use AUTH <pin> to login.` - Run AUTH first
- `ERROR: Wrong PIN. N retries remaining.` - Retry with correct PIN
- `ERROR: PIN locked. Wait N seconds.` - Wait and retry
- `ERROR: PIN permanently locked.` - Requires hardware reset

### Parameter Errors
- `ERROR: Invalid <field> number` - Check format
- `ERROR: Invalid <field> (min-max)` - Check range
- `ERROR: <field> not found` - Check existence

### Hardware Errors
- `ERROR: Secure Element not available` - Check connection
- `ERROR: Session start failed` - Run TR01_SESSION first
- `ERROR: Delete failed` - Slot may be empty

### Memory Errors
- `ERROR: out of memory` - Free unused slots, retry
- `ERROR: allocation failed` - Reduce batch size
```

## References
- components/serial_cmd/src/SerialCmd.cpp (error output locations)
- components/serial_cmd/src/CommandRegistry.cpp (auth errors)
- docs/SERIAL_COMMANDS.md (current documentation)
