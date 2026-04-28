---
title: "[MEDIUM] Duplicated token-parsing helper functions in serial command handlers"
severity: MEDIUM
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

Two identical helper functions (`skipSpaces` and `nextToken`) for parsing whitespace-delimited tokens from serial command arguments are duplicated across two module files:

- **File 1:** `components/mod_totp/src/TotpModule.cpp` (lines 105-129)
- **File 2:** `components/mod_password/src/PasswordModule.cpp` (lines 129-153)

The functions are functionally identical (80%+ code similarity) and serve the same purpose: parsing serial command arguments into tokens.

### Code Comparison

**TotpModule.cpp (lines 105-129):**
```cpp
static const char* skipSpaces(const char* s) {
    while (s && *s && std::isspace(static_cast<unsigned char>(*s))) {
        s++;
    }
    return s;
}

static const char* nextToken(const char* s, char* out, size_t outSize) {
    if (!out || outSize == 0) return nullptr;
    s = skipSpaces(s);
    if (!s || !*s) return nullptr;
    size_t i = 0;
    while (*s && !std::isspace(static_cast<unsigned char>(*s)) && i + 1 < outSize) {
        out[i++] = *s++;
    }
    out[i] = '\0';
    return s;
}
```

**PasswordModule.cpp (lines 129-153):**
```cpp
static const char* skipSpaces(const char* s) {
    while (s && *s && std::isspace(static_cast<unsigned char>(*s))) {
        s++;
    }
    return s;
}

static const char* nextToken(const char* s, char* out, size_t outSize) {
    if (!out || outSize == 0) return nullptr;
    s = skipSpaces(s);
    if (!s || !*s) return nullptr;
    size_t i = 0;
    while (*s && !std::isspace(static_cast<unsigned char>(*s)) && i + 1 < outSize) {
        out[i++] = *s++;
    }
    out[i] = '\0';
    return s;
}
```

## Impact

- **Maintenance Burden:** Any bug fix or improvement to token parsing must be applied in two places, increasing the risk of inconsistencies.
- **Code Bloat:** Approximately 50 lines of duplicated code increase the binary size.
- **DRY Violation:** Violates the "Don't Repeat Yourself" principle, making the codebase harder to maintain.
- **Future Risk:** When additional modules add serial commands (e.g., `mod_fido2`, `mod_gpg`), they may duplicate these functions again.

## Evidence

**Usage in TotpModule.cpp:**
- Line 235: `const char* p = nextToken(args, name, sizeof(name));`
- Line 240-248: Multiple `nextToken` calls for parsing TOTP_ADD command
- Line 220: `const char* p = nextToken(args, indexBuf, sizeof(indexBuf));` for TOTP_DEL

**Usage in PasswordModule.cpp:**
- Line 262-274: Multiple `nextToken` calls for parsing PASSWORD_ADD command
- Line 308: `const char* p = nextToken(args, indexBuf, sizeof(indexBuf));` for PASSWORD_DEL
- Line 276: `const char* notes = skipSpaces(p);` for notes parsing

## Recommended Fix

1. **Create a shared utility header** `components/cdc_core/include/cdc_core/StringUtils.h`:
   ```cpp
   #pragma once
   #include <cstddef>
   
   namespace cdc::core {
   
   /**
    * \brief Advances over leading ASCII whitespace in a C string.
    * \param s Input string pointer.
    * \return Pointer to first non-whitespace character.
    */
   inline const char* skipSpaces(const char* s) {
       while (s && *s && std::isspace(static_cast<unsigned char>(*s))) {
           s++;
       }
       return s;
   }
   
   /**
    * \brief Extracts one whitespace-delimited token from a string.
    * \param s Input cursor position.
    * \param out Output token buffer.
    * \param outSize Output buffer size.
    * \return Pointer to the next unread input position or `nullptr` if no token exists.
    */
   inline const char* nextToken(const char* s, char* out, size_t outSize) {
       if (!out || outSize == 0) return nullptr;
       s = skipSpaces(s);
       if (!s || !*s) return nullptr;
       size_t i = 0;
       while (*s && !std::isspace(static_cast<unsigned char>(*s)) && i + 1 < outSize) {
           out[i++] = *s++;
       }
       out[i] = '\0';
       return s;
   }
   
   } // namespace cdc::core
   ```

2. **Update `components/mod_totp/src/TotpModule.cpp`:**
   - Add `#include "cdc_core/StringUtils.h"`
   - Replace `skipSpaces` → `cdc::core::skipSpaces`
   - Replace `nextToken` → `cdc::core::nextToken`
   - Remove local function definitions (lines 105-129)

3. **Update `components/mod_password/src/PasswordModule.cpp`:**
   - Add `#include "cdc_core/StringUtils.h"`
   - Replace `skipSpaces` → `cdc::core::skipSpaces`
   - Replace `nextToken` → `cdc::core::nextToken`
   - Remove local function definitions (lines 129-153)

## References

- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- C++ Inline Functions: [cppreference](https://en.cppreference.com/w/cpp/language/inline)
- Related: Similar patterns exist in `mod_fido2` and `mod_gpg` command handlers that could benefit from this utility
