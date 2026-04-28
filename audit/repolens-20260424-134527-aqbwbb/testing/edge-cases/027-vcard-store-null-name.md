---
title: "[MEDIUM] vcard_store_parse_names can produce empty display name with edge-case N: field"
severity: MEDIUM
domain: mod_vcard/vcard_store
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `vcard_store.cpp:199-267`, the `vcard_parse_names` function can produce an empty display name in certain edge cases when the N: field has specific malformed formats.

The function handles the N: field (formatted as `Family;Given;Middle;Prefix;Suffix`) but when all fields are empty or contain only delimiters, the fallback to "vCard" happens, but intermediate edge cases can produce unexpected results.

## Impact
- **Data quality**: Empty or malformed display names can propagate through the system
- **UI rendering**: Lists may show blank entries or unexpected formatting
- **Sorting**: Empty last names affect sort order unpredictably

## Evidence
File: `components/mod_vcard/src/vcard_store.cpp`, lines 199-267

```cpp
static void vcard_parse_names(const char* vcard, char* last, size_t last_len,
                              char* display, size_t display_len) {
    // ...
    if (n_line[0] != '\0') {
        char* family = n_line;
        char* given = strchr(n_line, ';');
        if (given) {
            *given = '\0';
            given++;
            char* next_semi = strchr(given, ';');
            if (next_semi) *next_semi = '\0';
        }
        if (family && *family) {
            strncpy(last, family, last_len - 1);
            last[last_len - 1] = '\0';
        }
        if (display[0] == '\0') {
            if (given && *given && family && *family) {
                snprintf(display, display_len, "%s %s", given, family);
            } else if (given && *given) {
                snprintf(display, display_len, "%s", given);
            } else if (family && *family) {
                snprintf(display, display_len, "%s", family);
            }
        }
    }
    // ...
    if (display[0] == '\0' && fn[0] != '\0') {
        strncpy(display, fn, display_len - 1);
        display[display_len - 1] = '\0';
    }
    // ...
    if (display[0] == '\0') {
        strncpy(display, "vCard", display_len - 1);
        display[display_len - 1] = '\0';
    }
}
```

Edge cases:
1. `N:;John;;` - Given is "John", family is empty, display becomes "John"
2. `N:;` - Both family and given are empty strings, but `n_line[0] != '\0'` is true
3. `N:;;;` - Multiple empty fields can cause `given` to point to empty string

## Recommended Fix
Add explicit checks for empty content in the N: field parsing:

```cpp
static void vcard_parse_names(const char* vcard, char* last, size_t last_len,
                              char* display, size_t display_len) {
    // ...
    if (n_line[0] != '\0') {
        char* family = n_line;
        char* given = strchr(n_line, ';');
        if (given) {
            *given = '\0';
            given++;
            char* next_semi = strchr(given, ';');
            if (next_semi) *next_semi = '\0';
        }
        
        // Check if family is truly non-empty (not just whitespace)
        bool hasFamily = family && *family;
        bool hasGiven = given && *given;
        
        if (hasFamily) {
            strncpy(last, family, last_len - 1);
            last[last_len - 1] = '\0';
        }
        if (display[0] == '\0') {
            if (hasGiven && hasFamily) {
                snprintf(display, display_len, "%s %s", given, family);
            } else if (hasGiven) {
                snprintf(display, display_len, "%s", given);
            } else if (hasFamily) {
                snprintf(display, display_len, "%s", family);
            }
        }
    }
    // ...
}
```

## References
- RFC 6350: vCard Format Specification
- CWE-131: Incorrect Calculation of Multi-Byte String Length
