---
title: "[MEDIUM] Mixed case styles for constants: SCREAMING_SNAKE_CASE vs camelCase"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses inconsistent case styles for constant definitions. Some constants use `SCREAMING_SNAKE_CASE` while others use `camelCase`, particularly in header files defining module-specific constants.

**Evidence:**

1. **components/mod_password/include/mod_password/PasswordStore.h** (lines 9-19):
   ```cpp
   constexpr uint8_t PASSWORD_TITLE_LEN = 24;
   constexpr uint8_t PASSWORD_USERNAME_LEN = 16;
   constexpr uint8_t PASSWORD_PASSWORD_LEN = 64;
   constexpr uint8_t PASSWORD_URL_LEN = 64;
   ```
   Uses `SCREAMING_SNAKE_CASE` for constexpr constants.

2. **components/mod_vcard/include/mod_vcard/vcard_store.h** (lines 6-7):
   ```cpp
   #define VCARD_MAX_LEN   768
   #define VCARD_MAX_CARDS 100
   ```
   Also uses `SCREAMING_SNAKE_CASE` but with `#define` instead of `constexpr`.

3. **components/cdc_views/include/cdc_views/RenderHelpers.h** (lines 9-10):
   ```cpp
   constexpr int kFooterHeight = 16;
   constexpr int kScrollIndicatorWidth = 8;
   ```
   Uses `camelCase` with `k` prefix (Google style).

4. **components/cdc_core/include/cdc_core/feature_flags.h** (lines 17-21):
   ```cpp
   #define FEATURE_SECURE_SERIAL 1
   #define FEATURE_NVS_EDIT 0
   #define DEBUG_MODE 1
   ```
   Uses `SCREAMING_SNAKE_CASE` for feature flags.

## Impact
- **Readability inconsistency**: Developers must switch mental contexts when reading different modules
- **Code search difficulty**: Searching for constants requires knowing which convention was used
- **Onboarding friction**: New developers need to learn multiple conventions
- **Maintenance burden**: Risk of accidentally using the wrong style when adding new constants

## Evidence
Multiple files showing different conventions:
- `components/cdc_views/include/cdc_views/RenderHelpers.h:9-10` - camelCase with k-prefix
- `components/mod_password/include/mod_password/PasswordStore.h:9-19` - SCREAMING_SNAKE_CASE with constexpr
- `components/mod_vcard/include/mod_vcard/vcard_store.h:6-7` - SCREAMING_SNAKE_CASE with #define
- `components/cdc_core/include/cdc_core/feature_flags.h:17-21` - SCREAMING_SNAKE_CASE for feature flags

## Recommended Fix
Choose ONE convention for all constants and apply consistently:

**Option A (Recommended for C++):** Use `SCREAMING_SNAKE_CASE` for all constants (matching ESP-IDF and existing codebase majority):
```cpp
constexpr uint8_t PASSWORD_TITLE_LEN = 24;
constexpr int FOOTER_HEIGHT = 16;
constexpr int SCROLL_INDICATOR_WIDTH = 8;
```

**Option B (Google C++ style):** Use `kCamelCase` for all constexpr constants:
```cpp
constexpr uint8_t kPasswordTitleLen = 24;
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;
```

**Steps:**
1. Update `components/cdc_views/include/cdc_views/RenderHelpers.h` to use `SCREAMING_SNAKE_CASE`
2. Update `components/mod_vcard/include/mod_vcard/vcard_store.h` to use `constexpr` instead of `#define`
3. Search the entire codebase for other deviations and fix them
4. Document the chosen convention in a style guide

## References
- [C++ Core Guidelines - Naming Rules](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html#Constant_Names)
- [ESP-IDF Coding Style](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#configuration-variables)
