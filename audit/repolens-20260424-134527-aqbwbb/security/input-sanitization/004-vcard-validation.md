---
title: "[MEDIUM] Insufficient vCard validation for multiline paste input"
severity: MEDIUM
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `vcardLineInterceptor` function in `components/mod_vcard/src/VcardModule.cpp` handles multiline vCard paste input via the serial command `VCARD_SET`. While it validates the vCard format using `vcard_validate()`, the validation is basic and does not check for:
1. Special characters that could cause display issues
2. Control characters (except NUL)
3. Excessively long individual lines
4. Potential injection of malicious content

**Location:** `components/mod_vcard/src/VcardModule.cpp:313-356` (vcardLineInterceptor)
**Location:** `components/mod_vcard/src/vcard_store.cpp:285-312` (vcard_validate)

## Impact
- **Special characters**: vCards can contain special characters that may not be properly escaped, potentially causing:
  - Display issues on the E-Paper display
  - Issues when exporting or sharing vCards
  - Potential injection of formatting codes
- **Control characters**: Only NUL is checked. Other control characters (CR, LF within lines, tabs, etc.) could cause parsing issues
- **Line length**: No validation of individual line lengths, which could lead to:
  - Buffer issues in downstream parsers
  - Display truncation
- **Content validation**: The validation checks for BEGIN/VERSION/END but doesn't verify that required fields (like N or FN) have valid content

## Evidence
```cpp
// components/mod_vcard/src/VcardModule.cpp:313-356
static bool vcardLineInterceptor(const char* line) {
    if (!s_vcardInputMode) return false;
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

```cpp
// components/mod_vcard/src/vcard_store.cpp:285-312
static bool vcard_validate(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard || len == 0) {
        set_err(err, err_len, "Empty vCard");
        return false;
    }
    if (len > VCARD_MAX_LEN) {
        set_err(err, err_len, "vCard too large");
        return false;
    }
    if (memchr(vcard, '\0', len) != nullptr) {
        set_err(err, err_len, "vCard contains NUL");
        return false;
    }
    if (!strstr(vcard, "BEGIN:VCARD")) {
        set_err(err, err_len, "Missing BEGIN:VCARD");
        return false;
    }
    if (!strstr(vcard, "VERSION:4.0")) {
        set_err(err, err_len, "Missing VERSION:4.0");
        return false;
    }
    if (!strstr(vcard, "END:VCARD")) {
        set_err(err, err_len, "Missing END:VCARD");
        return false;
    }
    return true;  // <-- No validation of field content or special characters
}
```

## Recommended Fix
Add additional validation in `vcard_validate()` to check for:
1. Control characters (except allowed ones like CR, LF for line endings)
2. Reasonable line lengths
3. Basic field content validation

```cpp
static bool vcard_validate(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard || len == 0) {
        set_err(err, err_len, "Empty vCard");
        return false;
    }
    if (len > VCARD_MAX_LEN) {
        set_err(err, err_len, "vCard too large");
        return false;
    }
    if (memchr(vcard, '\0', len) != nullptr) {
        set_err(err, err_len, "vCard contains NUL");
        return false;
    }
    if (!strstr(vcard, "BEGIN:VCARD")) {
        set_err(err, err_len, "Missing BEGIN:VCARD");
        return false;
    }
    if (!strstr(vcard, "VERSION:4.0")) {
        set_err(err, err_len, "Missing VERSION:4.0");
        return false;
    }
    if (!strstr(vcard, "END:VCARD")) {
        set_err(err, err_len, "Missing END:VCARD");
        return false;
    }
    
    // Check for control characters (allow CR, LF, TAB for formatting)
    for (size_t i = 0; i < len; i++) {
        char c = vcard[i];
        // Allow printable ASCII and common whitespace
        if (c >= 0x20 && c <= 0x7E) continue;  // Printable ASCII
        if (c == '\r' || c == '\n' || c == '\t') continue;  // Allowed whitespace
        // Check for other whitespace (space)
        if (c == ' ') continue;
        
        set_err(err, err_len, "vCard contains invalid character");
        return false;
    }
    
    // Check for reasonable line lengths (e.g., max 255 chars per line)
    size_t lineStart = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || vcard[i] == '\n') {
            size_t lineLen = i - lineStart;
            if (lineLen > 255) {
                set_err(err, err_len, "vCard line too long");
                return false;
            }
            lineStart = i + 1;
        }
    }
    
    return true;
}
```

## References
- RFC 6350: vCard Format Specification
- CWE-20: Improper Input Validation
- CWE-131: Incorrect Calculation of Buffer Size
- vCard 4.0 specification for field formatting rules
