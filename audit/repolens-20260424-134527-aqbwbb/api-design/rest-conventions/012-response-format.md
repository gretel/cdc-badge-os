---
title: "[LOW] Missing Response Status Indicators for Commands"
severity: LOW
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
Serial command responses use inconsistent status indicators, making it harder to programmatically parse command results.

**Locations:**
- `components/serial_cmd/src/SerialCmd.cpp:410-1188` - Command handlers
- `components/mod_totp/src/TotpModule.cpp:195-320` - TOTP commands
- `components/mod_password/src/PasswordModule.cpp:184-325` - Password commands
- `components/mod_gpg/src/GpgModule.cpp:130-200` - GPG commands

## Impact
**Parsing complexity:** Scripts and automation tools must handle multiple response formats.

**Error detection:** Users must parse full output to determine success/failure rather than checking a simple status indicator.

**Extensibility:** New commands may continue the inconsistent pattern.

## Evidence

### Response Format Inconsistencies:

1. **Simple OK/ERROR responses:**
   ```cpp
   // TOTP_ADD (TotpModule.cpp:262)
   cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");

   // PASSWORD_ADD (PasswordModule.cpp:298)
   cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");

   // GPG_RESET (GpgModule.cpp:200)
   cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
   ```

2. **Detailed output with implicit success:**
   ```cpp
   // TOTP_LIST (TotpModule.cpp:195-220)
   cdc::serial::Console::printf("%u: %s (slot %u)\r\n", c->idx, entry.name, logical);
   // No explicit OK at end, implicit success if data shown

   // GPG_STATUS (GpgModule.cpp:131-139)
   cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
   cdc::serial::Console::printf("Curve: %s\r\n", ...);
   // No OK at end
   ```

3. **ERROR with details:**
   ```cpp
   // TR01_RMEM_READ (SerialCmd.cpp:1005-1014)
   Console::printf("ERROR: Read failed (slot may be empty)\r\n");

   // SET_TIME (SerialCmd.cpp:704)
   Console::printf("ERROR: Invalid format. Use HH:MM:SS\r\n");
   ```

4. **Mixed success indicators:**
   ```cpp
   // SET_TIME success (SerialCmd.cpp:719)
   Console::printf("OK: Time set to %02d:%02d:%02d\r\n", h, m, s);

   // TOTP_ADD success (TotpModule.cpp:262)
   Console::printf("OK\r\n");

   // TR01_SESSION success (SerialCmd.cpp:954)
   Console::printf("OK: Session started\r\n");
   ```

### Current Response Patterns:

| Command | Success Format | Failure Format |
|---------|---------------|----------------|
| TOTP_ADD | `OK` | `ERROR` |
| TOTP_DEL | `OK` | `ERROR` or `ERROR: index not found` |
| TOTP_GET | `<code> (30s)` | `ERROR: time not valid` |
| PASSWORD_ADD | `OK` | `ERROR` |
| PASSWORD_GET | Full entry details | `ERROR: invalid index` |
| GPG_STATUS | Full status | `ERROR: No key configured` |
| GPG_GENERATE | `OK` | `ERROR` |
| SET_TIME | `OK: Time set to ...` | `ERROR: Invalid format` |
| TR01_WIPE | Detailed progress | `ERROR: Cannot start session` |

## Recommended Fix

### Standardize Response Format

Adopt a consistent response pattern:

```
<STATUS>: <message>

Where STATUS is one of:
- OK: Command succeeded
- INFO: Informational response (for read commands)
- WARN: Command succeeded with warnings
- ERROR: Command failed
```

### Implementation Example:

```cpp
// For simple success/failure:
Console::printf("OK\r\n");                    // Success
Console::printf("ERROR: <reason>\r\n");       // Failure

// For data responses:
Console::printf("INFO:\r\n");                 // Start of data
Console::printf("  <data line>\r\n");
Console::printf("END\r\n");                   // End marker

// For commands with side effects:
Console::printf("OK: <action completed>\r\n");
```

### Specific Changes:

1. **Add END marker to list commands:**
   ```cpp
   // TOTP_LIST
   cdc::serial::Console::printf("INFO:\r\n");
   cdc::serial::Console::printf("%u: %s (slot %u)\r\n", ...);
   cdc::serial::Console::printf("END\r\n");
   ```

2. **Standardize error messages:**
   ```cpp
   // Before:
   Console::printf("ERROR: index not found\r\n");

   // After:
   Console::printf("ERROR: NOT_FOUND\r\n");
   ```

3. **Add response codes for automation:**
   ```cpp
   // Optional: Add exit codes or response codes
   Console::printf("OK:200\r\n");           // Success
   Console::printf("ERROR:404:NOT_FOUND\r\n"); // Not found
   Console::printf("ERROR:400:INVALID_ARGS\r\n"); // Bad args
   ```

### Response Code Catalog:

Define standard response codes:
```
200: OK - Success
201: CREATED - Resource created (e.g., TOTP_ADD)
204: NO_CONTENT - Success with no data (e.g., DELETE)
400: INVALID_ARGS - Bad command arguments
404: NOT_FOUND - Resource not found
409: CONFLICT - Resource already exists
500: INTERNAL_ERROR - Server error
```

## References

- [HTTP Status Codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)
- [RFC 7231 - HTTP/1.1 Semantics](https://tools.ietf.org/html/rfc7231#section-6)
- [Google API Error Handling](https://cloud.google.com/apis/design/errors)
