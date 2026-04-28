## Summary
The password module's detail view (`components/mod_password/src/PasswordModule.cpp:457-515`) displays 6 distinct data fields in a single flat list without logical grouping: Title, Username, Password, URL, TOTP Slot, and Notes. All fields are shown at once with equal visual weight, creating information density that can overwhelm users on a small 296x128 display.

The view does not group related fields (e.g., credentials vs. metadata) or use progressive disclosure for secondary fields like Notes or TOTP Slot.

## Impact
On a small e-paper display with limited real estate, showing all 6 fields simultaneously can result in:
- Truncated text for longer values (URLs, notes)
- Visual clutter with no clear hierarchy
- Users struggling to find the primary information (password) among secondary details
- No clear distinction between required fields (title, password) and optional fields (URL, TOTP, notes)

## Evidence
File: `components/mod_password/src/PasswordModule.cpp:457-515`
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

All fields are rendered with the same formatting. The Notes field uses a multi-line format ("Notes:\n%s") which can push other fields off-screen if notes are long.

## Recommended Fix
1. Group fields into logical sections with visual headers:
   - "Credentials" section: Title, Username, Password
   - "Connection" section: URL
   - "Extras" section: TOTP Slot, Notes
2. Add truncation with "..." for long fields and show full value on select (or use InfoView's scroll feature)
3. Use indentation or spacing to show field hierarchy within sections

Approximate effort: 1 hour to refactor the detail text formatting with section headers and basic grouping.

## References
- PasswordModule detail view: `components/mod_password/src/PasswordModule.cpp:457-515`
- InfoView rendering: `components/cdc_views/src/InfoView.cpp:170-220`
