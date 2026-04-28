---
title: "[LOW] Password module details view lacks format hints for technical fields"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The password module's details view (`components/mod_password/src/PasswordModule.cpp:461-514`) displays password entry fields (Title, Username, Password, URL, TOTP Slot, Notes) but provides no hints about expected formats for fields like URL or TOTP Slot.

**Evidence** (`components/mod_password/src/PasswordModule.cpp:487-499`):
```cpp
snprintf(detailText, sizeof(detailText),
         "Title: %s\n"
         "Username: %s\n"
         "Password: %s\n"
         "URL: %s\n"
         "TOTP Slot: %s\n"
         "Notes:\n%s",
         entry.title,
         usernameText,
         passwordText,
         urlText,
         totpText,
         notesText);
```

The URL field shows the raw value but doesn't indicate:
- Expected format (e.g., "https://example.com")
- Whether protocol is required
- What makes a valid URL

The TOTP Slot field shows a number but doesn't explain:
- What a TOTP slot is
- Valid range (1-100 based on allocation)
- How to set up TOTP

## Impact
Users viewing their password entries may be confused about:
1. Whether their URL format is correct
2. What the TOTP Slot means and how to use it
3. How to properly edit these fields later

## Evidence
- File: `components/mod_password/src/PasswordModule.cpp`
- Lines: 461-514 (showDetails function)
- Line 78: `STR_TOTP_SLOT` defined as "TOTP Slot (optional)" - minimal explanation
- No format examples or helper text in the UI

## Recommended Fix
Add inline format hints to the password details view:

1. **URL field**: Show format example
   ```
   URL: https://example.com (format: https://domain.com)
   ```

2. **TOTP Slot field**: Add explanation
   ```
   TOTP Slot: 5 (1-100, links to TOTP module)
   ```

3. **Add info icon action**: Key '3' could show a summary of field formats

## References
- URL format specification: https://tools.ietf.org/html/rfc3986
- TOTP algorithm: https://tools.ietf.org/html/rfc6238
