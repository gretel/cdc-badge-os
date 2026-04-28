---
title: "[MEDIUM] Inconsistent use of DO tag constants in cmd_put function"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The `cmd_put` function in `components/mod_gpg/src/openpgp/openpgp.cpp` uses inline hex values for some DO tags instead of the defined constants from `openpgp.h`. This creates inconsistency with the `cmd_get` function which uses the constants.

**Locations:** Lines 906, 917, 927 (inline hex values should use `DO_NAME`, `DO_LANG_PREF`, `DO_SEX`)

## Impact
- **Consistency**: `cmd_get` uses constants (`DO_NAME`, `DO_LANG_PREF`, `DO_SEX`) but `cmd_put` uses inline values (`0x005B`, `0x5F2D`, `0x5F35`).
- **Maintainability**: If tag values change, only one function needs updating, but developers must remember to update both.
- **Readability**: Using constants makes the code more self-documenting.

## Evidence

**Lines 906, 917, 927 (cmd_put - uses inline values):**
```cpp
switch (tag) {
    // Cardholder data (0x5B: Name, part of 0x65)
    case 0x005B:  // Should be DO_NAME
        ...

    // Language preference
    case 0x5F2D:  // Should be DO_LANG_PREF
        ...

    // Sex
    case 0x5F35:  // Should be DO_SEX
        ...
}
```

**Lines 717-830 (cmd_get - uses constants):**
```cpp
case DO_NAME:       // 0x5B: Cardholder name
case DO_LANG_PREF:  // 0x5F2D: Language preference
case DO_SEX:        // 0x5F35: Sex
```

**Defined constants in `components/mod_gpg/include/mod_gpg/openpgp/openpgp.h`:**
```cpp
#define DO_NAME             0x005B  // Name (Cardholder)
#define DO_LANG_PREF        0x5F2D  // Language preference
#define DO_SEX              0x5F35  // Sex
```

## Recommended Fix

Replace inline hex values with constants in `cmd_put` function:

```cpp
// Before:
switch (tag) {
    // Cardholder data (0x5B: Name, part of 0x65)
    case 0x005B:
        ...

    // Language preference
    case 0x5F2D:
        ...

    // Sex
    case 0x5F35:
        ...
}

// After:
switch (tag) {
    // Cardholder data (0x5B: Name, part of 0x65)
    case DO_NAME:
        ...

    // Language preference
    case DO_LANG_PREF:
        ...

    // Sex
    case DO_SEX:
        ...
}
```

## References
- `components/mod_gpg/include/mod_gpg/openpgp/openpgp.h`: DO tag constants definition
- `components/mod_gpg/src/openpgp/openpgp.cpp`: cmd_get and cmd_put functions
