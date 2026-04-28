---
title: "[LOW] Missing URL format validation for password entries"
severity: LOW
domain: basic-validation
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `cmd_password_add` function in `components/mod_password/src/PasswordModule.cpp` accepts a URL field for password entries but does not validate that the input is a valid URL format. Any string is accepted and stored, which could lead to inconsistent data and potential issues when the URL is used for display or copying.

**Location:** `components/mod_password/src/PasswordModule.cpp:245-299`
**Location:** `components/mod_password/src/PasswordModule.cpp:611-618` (wizard URL handling)

## Impact
- **Data consistency**: URLs without proper format (e.g., missing protocol) may not be copy-paste usable
- **Display issues**: Malformed URLs could look confusing in the UI
- **User experience**: No feedback when users accidentally enter invalid URLs
- **Silent acceptance**: The command accepts any string without validation

## Evidence
```cpp
// components/mod_password/src/PasswordModule.cpp:245-299
static void cmd_password_add(const char* args) {
    PasswordEntry entry = {};
    entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;

    char title[PasswordStore::TITLE_LEN + 1] = {};
    char username[PasswordStore::USERNAME_LEN + 1] = {};
    char password[PasswordStore::PASSWORD_LEN + 1] = {};
    char url[PasswordStore::URL_LEN + 1] = {};
    char totpBuf[8] = {};

    const char* p = nextToken(args, title, sizeof(title));
    // ... parsing code ...
    p = nextToken(p, url, sizeof(url));
    // ... more parsing ...

    if (strcmp(url, "-") != 0) {
        strncpy(entry.url, url, sizeof(entry.url) - 1);  // No validation
    }

    bool ok = PasswordStore::instance().addEntry(entry);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

In the wizard at line 611-618:
```cpp
static void onWizardUrl(const char* text) {
    strncpy(s_wizard.entry.url, text ? text : "", sizeof(s_wizard.entry.url) - 1);
    // ... advance to next step without validation ...
}
```

## Recommended Fix
Add basic URL format validation to ensure the URL has a reasonable structure:

```cpp
// Helper function for basic URL validation
static bool isValidUrl(const char* url) {
    if (!url || !*url) return true;  // Empty URL is OK (optional field)
    
    size_t len = strlen(url);
    if (len > PasswordStore::URL_LEN) return false;
    
    // Check for basic URL structure
    // At minimum, should have a protocol or start with www or contain a dot
    bool hasProtocol = strstr(url, "://") != nullptr;
    bool hasWww = strncmp(url, "www.", 4) == 0;
    bool hasDot = strchr(url, '.') != nullptr;
    
    // Should have at least one of these patterns
    if (!hasProtocol && !hasWww && !hasDot) {
        return false;
    }
    
    // Check for control characters
    for (size_t i = 0; i < len; i++) {
        char c = url[i];
        if (c < 0x20 || c > 0x7E) {  // Printable ASCII only
            return false;
        }
    }
    
    return true;
}

// In cmd_password_add():
if (strcmp(url, "-") != 0) {
    if (!isValidUrl(url)) {
        cdc::serial::Console::printf("ERROR: Invalid URL format\r\n");
        return;
    }
    strncpy(entry.url, url, sizeof(entry.url) - 1);
}
```

For the wizard:
```cpp
static void onWizardUrl(const char* text) {
    strncpy(s_wizard.entry.url, text ? text : "", sizeof(s_wizard.entry.url) - 1);
    
    // Optional: Validate and show error if needed
    if (text && text[0] && !isValidUrl(text)) {
        ui::showToastError("Invalid URL format");
        // Optionally re-prompt or continue
    }
    
    // ... advance to next step ...
}
```

## References
- RFC 3986: Uniform Resource Identifier (URI)
- CWE-20: Improper Input Validation
- Common URL format patterns for validation
