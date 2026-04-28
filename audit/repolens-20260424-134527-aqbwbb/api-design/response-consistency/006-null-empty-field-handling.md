---
title: "[MEDIUM] Password module serial commands don't handle empty fields consistently with UI"
severity: MEDIUM
domain: api-design
lens: response-consistency
labels:
  - "audit:api-design/response-consistency"
  - "serial-commands"
  - "mod_password"
  - "data-format"
---

## Summary

The Password module's serial command `PASSWORD_GET` outputs raw empty strings for optional fields, while the UI detail view wraps empty fields with `(empty)` for better readability. This creates inconsistent user experience and harder-to-parse serial output.

**Locations:**
- `components/mod_password/src/PasswordModule.cpp` - Lines 236-245 (serial output)
- `components/mod_password/src/PasswordModule.cpp` - Lines 482-499 (UI output)

**Inconsistency:**

| Field | Serial Output (PASSWORD_GET) | UI Detail View |
|-------|------------------------------|----------------|
| Title | Raw value (always has value) | Raw value |
| Username | Empty string if blank | `(empty)` |
| Password | Empty string if blank | `(empty)` |
| URL | Empty string if blank | `(empty)` |
| TOTP Slot | `none` | `(empty)` |
| Notes | Empty string if blank | `(empty)` |

## Impact

**User experience inconsistency:** Users see different representations of empty data depending on how they access it:
- Serial: `Username: ` (blank, confusing)
- UI: `Username: (empty)` (clear)

**Parser difficulty:** External tools parsing serial output cannot easily distinguish between:
- Empty string value
- Missing value
- Null value

**Visual alignment:** In the UI, all fields have consistent width, but serial output has varying line lengths.

**Data clarity:** Empty fields in serial output look like missing data rather than intentional empty values.

## Evidence

**Serial command output (lines 236-245):**
```cpp
// Lines 236-238 - Raw values, no empty handling
cdc::serial::Console::printf("Title: %s\r\n", entry.title);
cdc::serial::Console::printf("Username: %s\r\n", entry.username);
cdc::serial::Console::printf("Password: %s\r\n", entry.password);

// Line 239 - Raw URL
cdc::serial::Console::printf("URL: %s\r\n", entry.url);

// Lines 240-243 - Special handling for TOTP Slot
if (entry.totpSlot == PasswordStore::TOTP_SLOT_NONE) {
    cdc::serial::Console::printf("TOTP Slot: none\r\n");
} else {
    cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
}

// Line 245 - Raw notes
cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
```

**UI detail view (lines 474-499):**
```cpp
// Lines 474-476 - Create empty wrapper
const char* emptyText = ui::tr(ui::StringId::EMPTY);
char emptyWrapped[16] = {};
snprintf(emptyWrapped, sizeof(emptyWrapped), "(%s)", emptyText);

// Lines 477-481 - TOTP Slot handling
const char* totpText = emptyWrapped;
if (entry.totpSlot != PasswordStore::TOTP_SLOT_NONE) {
    snprintf(totpBuf, sizeof(totpBuf), "%u", entry.totpSlot);
    totpText = totpBuf;
}

// Lines 482-485 - Conditional empty handling for all fields
const char* usernameText = entry.username[0] ? entry.username : emptyWrapped;
const char* passwordText = entry.password[0] ? entry.password : emptyWrapped;
const char* urlText = entry.url[0] ? entry.url : emptyWrapped;
const char* notesText = entry.notes[0] ? entry.notes : emptyWrapped;

// Lines 487-499 - Use wrapped values
snprintf(detailText, sizeof(detailText),
         "Title: %s\n"
         "Username: %s\n"
         "Password: %s\n"
         "URL: %s\n"
         "TOTP Slot: %s\n"
         "Notes:\n%s",
         entry.title,         // Always has value
         usernameText,        // "(empty)" if blank
         passwordText,        // "(empty)" if blank
         urlText,             // "(empty)" if blank
         totpText,            // "(empty)" if none
         notesText);          // "(empty)" if blank
```

**Example output comparison:**

Serial command (`PASSWORD_GET 0`):
```
Title: google.com
Username: john@example.com
Password: secret123
URL: https://google.com
TOTP Slot: 5
Notes:
```

Same entry with empty optional fields:
```
Title: example.com
Username: 
Password: 
URL: 
TOTP Slot: none
Notes:
```

UI detail view (same empty entry):
```
Title: example.com
Username: (empty)
Password: (empty)
URL: (empty)
TOTP Slot: none
Notes:
(empty)
```

## Recommended Fix

### Standardize on `(empty)` for all optional fields in serial output

**Update PASSWORD_GET command (lines 236-245):**

```cpp
// Helper to format empty fields
static const char* formatEmpty(const char* value, const char* emptyStr) {
    return (value && value[0]) ? value : emptyStr;
}

// In cmd_password_get:
const char* emptyText = "empty";  // Or use ui::tr(ui::StringId::EMPTY)
char emptyWrapped[16];
snprintf(emptyWrapped, sizeof(emptyWrapped), "(%s)", emptyText);

// Format optional fields
const char* usernameText = formatEmpty(entry.username, emptyWrapped);
const char* passwordText = formatEmpty(entry.password, emptyWrapped);
const char* urlText = formatEmpty(entry.url, emptyWrapped);
const char* notesText = formatEmpty(entry.notes, emptyWrapped);

// Output with consistent handling
cdc::serial::Console::printf("Title: %s\r\n", entry.title);
cdc::serial::Console::printf("Username: %s\r\n", usernameText);
cdc::serial::Console::printf("Password: %s\r\n", passwordText);
cdc::serial::Console::printf("URL: %s\r\n", urlText);
if (entry.totpSlot == PasswordStore::TOTP_SLOT_NONE) {
    cdc::serial::Console::printf("TOTP Slot: %s\r\n", emptyWrapped);  // Use (empty) instead of none
} else {
    cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
}
cdc::serial::Console::printf("Notes: %s\r\n", notesText);
```

### Alternative: Use explicit markers

If `(empty)` is too verbose for serial output, use a consistent marker:

```cpp
// Option 1: Use `-` for empty values
cdc::serial::Console::printf("Username: %s\r\n", entry.username[0] ? entry.username : "-");

// Option 2: Use `<empty>` tag
cdc::serial::Console::printf("Username: %s\r\n", entry.username[0] ? entry.username : "<empty>");

// Option 3: Use JSON-style null
cdc::serial::Console::printf("Username: %s\r\n", entry.username[0] ? entry.username : "null");
```

### Best practice: Create a helper function

```cpp
// In PasswordModule.cpp (near top, after helper functions)
/**
 * \brief Formats a string field, showing placeholder for empty values.
 * \param value String value to format.
 * \param buf Buffer for formatted output.
 * \param bufSize Buffer size.
 * \param placeholder Placeholder text for empty values.
 * \return Formatted string (value or placeholder).
 */
static const char* formatField(const char* value, char* buf, size_t bufSize, const char* placeholder) {
    if (value && value[0]) {
        return value;
    }
    snprintf(buf, bufSize, "(%s)", placeholder);
    return buf;
}

// Usage in cmd_password_get:
char buf1[16], buf2[16], buf3[16], buf4[16];
cdc::serial::Console::printf("Title: %s\r\n", entry.title);
cdc::serial::Console::printf("Username: %s\r\n", formatField(entry.username, buf1, sizeof(buf1), "empty"));
cdc::serial::Console::printf("Password: %s\r\n", formatField(entry.password, buf2, sizeof(buf2), "empty"));
cdc::serial::Console::printf("URL: %s\r\n", formatField(entry.url, buf3, sizeof(buf3), "empty"));
cdc::serial::Console::printf("TOTP Slot: %s\r\n", entry.totpSlot != PasswordStore::TOTP_SLOT_NONE 
                            ? snprintf(buf4, sizeof(buf4), "%u", entry.totpSlot), buf4
                            : formatField(nullptr, buf4, sizeof(buf4), "empty"));
cdc::serial::Console::printf("Notes: %s\r\n", formatField(entry.notes, buf4, sizeof(buf4), "empty"));
```

## References

- Existing finding: `011-date-time-format-inconsistencies.md` (UI consistency)
- JSON null conventions
- CLI data formatting best practices
