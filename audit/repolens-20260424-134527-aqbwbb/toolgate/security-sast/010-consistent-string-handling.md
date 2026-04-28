---
title: "[LOW] Inconsistent use of strcpy vs snprintf for string initialization"
severity: LOW
domain: security-sast
lens: toolgate
labels:
  - "CWE-120: Buffer Copy without Checking Size of Input"
---

## Summary
The codebase uses `strcpy()` for some string initializations where `snprintf()` or `strncpy()` would be more consistent and safer. While the current usages are safe (known constant sources), using `strcpy()` creates inconsistency and potential for future bugs.

**Locations:**
- `components/mod_totp/src/TotpStore.cpp:489` - `strcpy(codeOut, "------")`
- `components/cdc_os_ui/src/views/LockScreenView.cpp:68` - `strcpy(clock_, "--:--")`
- `components/cdc_os_ui/src/views/LockScreenView.cpp:127` - `strcpy(clock_, "--:--")`

## Impact
- **Inconsistency**: Other parts of the code use `snprintf()` with explicit sizes
- **Future bugs**: If the source string changes, `strcpy()` won't protect against overflow
- **Code review**: Requires reviewers to manually verify buffer sizes

## Evidence

### TotpStore.cpp:489
```cpp
if (!isTimeValid()) {
    strcpy(codeOut, "------");  // Should be: snprintf(codeOut, 9, "------");
    return -1;
}
```

### LockScreenView.cpp:68,127
```cpp
void LockScreenView::init() {
    // ...
    strcpy(clock_, "--:--");  // Should be: snprintf(clock_, sizeof(clock_), "--:--");
    // ...
}

void LockScreenView::setClock(const char* clock) {
    if (clock) {
        strncpy(clock_, clock, sizeof(clock_) - 1);
        clock_[sizeof(clock_) - 1] = '\0';
    } else {
        strcpy(clock_, "--:--");  // Should be: snprintf(clock_, sizeof(clock_), "--:--");
    }
    // ...
}
```

Note: `setClock()` already uses `strncpy()` for the non-else branch, making the `strcpy()` in the else branch inconsistent.

## Recommended Fix
Replace `strcpy()` with `snprintf()` for consistency:

```cpp
// TotpStore.cpp:489
snprintf(codeOut, 9, "------");

// LockScreenView.cpp:68,127
snprintf(clock_, sizeof(clock_), "--:--");
```

Alternatively, use `strncpy()` with explicit null-termination:
```cpp
strncpy(clock_, "--:--", sizeof(clock_) - 1);
clock_[sizeof(clock_) - 1] = '\0';
```

## References
- CWE-120: Buffer Copy without Checking Size of Input - https://cwe.mitre.org/data/definitions/120.html
- C Standard Library `snprintf()` - https://en.cppreference.com/w/c/io/fprintf
