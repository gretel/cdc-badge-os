---
title: "[HIGH] Hardcoded default PIN values in source code"
severity: HIGH
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
Default PIN values are hardcoded directly in `components/cdc_core/include/cdc_core/PinManager.h` as compile-time constants. These include:
- Badge/FIDO2 PIN: `"123456"`
- OpenPGP PW1 (User): `"123456"`
- OpenPGP PW3 (Admin): `"12345678"`

While these are documented as defaults, they are embedded in the source with no mechanism to configure them per environment or deployment.

## Impact
- **Security risk**: Default PINs are well-known and must be changed immediately after first use
- **No environment-specific configuration**: Development, staging, and production all use the same defaults
- **Hardcoded constants**: Changing defaults requires code modification and rebuild
- **Documentation dependency**: Users must read docs to know defaults exist and change them

## Evidence
File: `components/cdc_core/include/cdc_core/PinManager.h` lines 53-55:
```cpp
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

File: `components/cdc_core/include/cdc_core/PinManager.h` lines 26-28:
```
Defaults:
- Badge/FIDO2: "123456"
- OpenPGP PW1 (User): "123456" (min 6 digits)
- OpenPGP PW3 (Admin): "12345678" (min 8 digits)
```

## Recommended Fix
Move PIN defaults to configurable build flags:

1. Add to `components/cdc_core/include/cdc_core/feature_flags.h`:
   ```cpp
   #ifndef DEFAULT_BADGE_PIN
   #define DEFAULT_BADGE_PIN "123456"
   #endif
   #ifndef DEFAULT_PW1
   #define DEFAULT_PW1 "123456"
   #endif
   #ifndef DEFAULT_PW3
   #define DEFAULT_PW3 "12345678"
   #endif
   ```

2. Update `PinManager.h` to use preprocessor macros:
   ```cpp
   static constexpr const char* DEFAULT_BADGE_PIN = DEFAULT_BADGE_PIN;
   ```

3. Document in `platformio.ini`:
   ```ini
   build_flags =
       -DDEFAULT_BADGE_PIN="123456"
       -DDEFAULT_PW1="123456"
       -DDEFAULT_PW3="12345678"
   ```

4. Create `docs/CONFIGURATION.md` section documenting these flags

This allows environment-specific defaults without code changes.

## References
- [OWASP Default Credentials](https://cheatsheetseries.owasp.org/cheatsheets/Default_Credentials_Cheat_Sheet.html)
- [ESP-IDF Kconfig for build options](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/kconfig.html)
