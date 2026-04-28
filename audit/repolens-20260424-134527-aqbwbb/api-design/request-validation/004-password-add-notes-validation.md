---
title: "[LOW] PASSWORD_ADD command lacks field length validation for notes"
severity: LOW
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `PASSWORD_ADD` serial command in `components/mod_password/src/PasswordModule.cpp:294` copies the notes field without validating its length. While `strncpy` prevents buffer overflow, there's no feedback to the user if their notes were truncated.

**File**: `components/mod_password/src/PasswordModule.cpp`  
**Line**: 294  
**Function**: `cmd_password_add()`

## Impact
- **Silent data loss**: Notes longer than the buffer are truncated without warning
- **User confusion**: Users may think their full notes were saved when they weren't
- **Inconsistent validation**: Other fields (title, username, password, url) have the same issue

## Evidence
From `components/mod_password/src/PasswordModule.cpp:253-298`:

```cpp
char title[PasswordStore::TITLE_LEN + 1] = {};
char username[PasswordStore::USERNAME_LEN + 1] = {};
char password[PasswordStore::PASSWORD_LEN + 1] = {};
char url[PasswordStore::URL_LEN + 1] = {};
char totpBuf[8] = {};

// ... token parsing with nextToken() limiting each field ...

const char* notes = skipSpaces(p);  // No length check!

// ... copying fields ...
if (notes && notes[0]) {
    strncpy(entry.notes, notes, sizeof(entry.notes) - 1);  // No validation!
}
```

From `components/mod_password/include/mod_password/PasswordStore.h:9-18`:
```cpp
constexpr uint8_t PASSWORD_TITLE_LEN = 24;
constexpr uint8_t PASSWORD_USERNAME_LEN = 16;
constexpr uint8_t PASSWORD_PASSWORD_LEN = 64;
constexpr uint8_t PASSWORD_URL_LEN = 64;
// Notes length is calculated from payload size
```

The `nextToken()` function (line 119-128) limits each explicitly parsed field, but notes is taken as the remainder of the input string with `skipSpaces(p)`, which could be very long.

Problematic input:
```
PASSWORD_ADD "My Site" "user" "pass" "url" "1" "Very long notes that exceeds the notes buffer size and gets silently truncated..."
```

## Recommended Fix
Add length validation for the notes field:

```cpp
const char* notes = skipSpaces(p);

// ... existing field copying ...

if (notes && notes[0]) {
    size_t notesLen = strlen(notes);
    if (notesLen > sizeof(entry.notes) - 1) {
        cdc::serial::Console::printf("WARNING: Notes truncated to %zu characters\r\n", 
                                     sizeof(entry.notes) - 1);
    }
    strncpy(entry.notes, notes, sizeof(entry.notes) - 1);
    entry.notes[sizeof(entry.notes) - 1] = '\0';  // Ensure null termination
}
```

For a more user-friendly approach, warn before truncating:
```cpp
if (notes && notes[0]) {
    size_t notesLen = strlen(notes);
    if (notesLen > sizeof(entry.notes) - 1) {
        cdc::serial::Console::printf("ERROR: Notes too long (max %zu chars)\r\n", 
                                     sizeof(entry.notes) - 1);
        return;
    }
    strncpy(entry.notes, notes, sizeof(entry.notes) - 1);
}
```

## References
- Similar pattern in `components/mod_vcard/src/vcard_store.cpp:346` (vcard_store_set_own with error reporting)
- Password store header: `components/mod_password/include/mod_password/PasswordStore.h`
