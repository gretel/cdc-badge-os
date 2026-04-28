---
title: "[MEDIUM] VCARD_SET command lacks incremental validation for multiline input"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `VCARD_SET` serial command in `components/mod_vcard/src/VcardModule.cpp` accumulates multiline vCard input without incremental validation. Validation only occurs after all lines are collected, making it difficult for users to identify which line caused an error.

**File**: `components/mod_vcard/src/VcardModule.cpp`  
**Lines**: 320-357 (vcardLineInterceptor), 364-369 (cmdVcardSet)  
**Functions**: `vcardLineInterceptor()`, `cmdVcardSet()`

## Impact
- **Poor UX**: Users paste a large vCard, get a generic "Invalid vCard" error without knowing which line failed
- **No early feedback**: Invalid lines are accumulated until the end, wasting buffer space
- **Debug difficulty**: Hard to identify the specific problem in a multi-line vCard

## Evidence
From `components/mod_vcard/src/VcardModule.cpp:320-357`:

```cpp
static bool vcardLineInterceptor(const char* line) {
    using Console = serial::Console;

    // "---" terminates paste mode
    if (strncmp(line, "---", 3) == 0) {
        s_vcardBuf[s_vcardBufPos] = '\0';

        char err[64] = {};
        if (vcard_store_set_own(s_vcardBuf, static_cast<size_t>(s_vcardBufPos), err, sizeof(err))) {
            Console::printf("OK: vCard updated\r\n");
        } else {
            Console::printf("ERROR: %s\r\n", err[0] ? err : "Invalid vCard");  // Generic error!
        }

        // ... reset state ...
        return true;
    }

    // Append line + newline to buffer
    size_t lineLen = strlen(line);
    if (s_vcardBufPos + static_cast<int>(lineLen) + 2 < static_cast<int>(sizeof(s_vcardBuf))) {
        memcpy(s_vcardBuf + s_vcardBufPos, line, lineLen);
        s_vcardBufPos += static_cast<int>(lineLen);
        s_vcardBuf[s_vcardBufPos++] = '\n';
    } else {
        Console::printf("ERROR: vCard too large\r\n");
        // ... reset ...
    }
    return true;
}
```

From `components/mod_vcard/src/vcard_store.cpp:288-314` (vcard_validate):
```cpp
static bool vcard_validate(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard || len == 0) {
        set_err(err, err_len, "Empty vCard");
        return false;
    }
    // ... checks for BEGIN:VCARD, VERSION:4.0, END:VCARD ...
    return true;
}
```

The validation happens only once at the end, checking:
- Not empty
- Not too large
- Contains BEGIN:VCARD, VERSION:4.0, END:VCARD

But doesn't validate:
- Individual line format
- Required fields (N:, FN:)
- Field value formats (phone numbers, emails, URLs)
- Line-by-line syntax

## Recommended Fix
Add incremental line validation:

```cpp
static bool vcardLineInterceptor(const char* line) {
    using Console = serial::Console;

    // "---" terminates paste mode
    if (strncmp(line, "---", 3) == 0) {
        s_vcardBuf[s_vcardBufPos] = '\0';

        char err[64] = {};
        if (vcard_store_set_own(s_vcardBuf, static_cast<size_t>(s_vcardBufPos), err, sizeof(err))) {
            Console::printf("OK: vCard updated\r\n");
        } else {
            Console::printf("ERROR: %s\r\n", err[0] ? err : "Invalid vCard");
        }

        // ... reset state ...
        return true;
    }

    // Validate line before appending
    size_t lineLen = strlen(line);
    
    // Check line length (vCard lines should not be excessively long)
    if (lineLen > 1000) {
        Console::printf("ERROR: Line too long (max 1000 chars)\r\n");
        // ... reset ...
        return true;
    }
    
    // Basic line format check (should be KEY:VALUE or KEY;PARAMS:VALUE)
    if (lineLen > 0 && line[0] != ' ' && line[0] != '\t') {
        const char* colon = strchr(line, ':');
        if (!colon) {
            Console::printf("WARNING: Line without ':' (might be continuation): %s\r\n", line);
        }
    }
    
    // Append line + newline to buffer
    if (s_vcardBufPos + static_cast<int>(lineLen) + 2 < static_cast<int>(sizeof(s_vcardBuf))) {
        memcpy(s_vcardBuf + s_vcardBufPos, line, lineLen);
        s_vcardBufPos += static_cast<int>(lineLen);
        s_vcardBuf[s_vcardBufPos++] = '\n';
    } else {
        Console::printf("ERROR: vCard too large\r\n");
        // ... reset ...
    }
    return true;
}
```

Alternatively, provide better error context at the end:
```cpp
if (vcard_store_set_own(s_vcardBuf, static_cast<size_t>(s_vcardBufPos), err, sizeof(err))) {
    Console::printf("OK: vCard updated\r\n");
} else {
    Console::printf("ERROR: %s\r\n", err[0] ? err : "Invalid vCard");
    Console::printf("Hint: Check for missing BEGIN:VCARD, VERSION:4.0, or END:VCARD\r\n");
    Console::printf("Paste format:\r\n");
    Console::printf("BEGIN:VCARD\r\n");
    Console::printf("VERSION:4.0\r\n");
    Console::printf("N:Last;First\r\n");
    Console::printf("FN:First Last\r\n");
    Console::printf("...\r\n");
    Console::printf("---\r\n");
}
```

## References
- vCard 4.0 spec: https://tools.ietf.org/html/rfc6350
- Similar validation pattern in `components/mod_vcard/src/vcard_store.cpp:129` (vcard_filter_empty_fields)
