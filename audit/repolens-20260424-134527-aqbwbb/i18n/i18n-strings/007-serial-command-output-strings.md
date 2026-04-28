---
title: "[LOW] Serial command output uses hardcoded English strings"
severity: LOW
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
Serial command handlers in GPG and other modules use hardcoded English strings for console output. While serial commands are primarily used by developers (who typically work in English), consistency would be improved by using i18n.

**components/mod_gpg/src/GpgModule.cpp:139-143**
```cpp
cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
cdc::serial::Console::printf("Curve: %s\r\n",
                             status.curve == CDC_CURVE_ED25519 ? "Ed25519" : "P-256");
cdc::serial::Console::printf("Created: %lu\r\n", static_cast<unsigned long>(status.created_at));
cdc::serial::Console::printf("Sign Count: %lu\r\n", static_cast<unsigned long>(status.sign_count));
```

**components/mod_gpg/src/GpgModule.cpp:158-159,168-169**
```cpp
cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
```

## Impact
- Serial output is English-only (less critical as it's developer-focused)
- Inconsistency between UI and serial command output
- German developers might prefer localized serial output

## Evidence
```
components/mod_gpg/src/GpgModule.cpp:139-143: Hardcoded status output labels
components/mod_gpg/src/GpgModule.cpp:158-159: Hardcoded usage message
components/mod_gpg/src/GpgModule.cpp:168-169: Hardcoded usage message
```

## Recommended Fix
**Option 1: Keep as-is (recommended for serial commands)**
Serial commands are primarily used by developers and technicians who typically work in English. The cost of i18n for serial output may not be worth the benefit.

**Option 2: Add simple i18n for serial output**
If localization is desired for serial output:

1. Add string IDs for common labels:
   ```cpp
   static constexpr uint16_t STR_SERIAL_USER_ID = 17;
   static constexpr uint16_t STR_SERIAL_CURVE = 18;
   static constexpr uint16_t STR_SERIAL_CREATED = 19;
   static constexpr uint16_t STR_SERIAL_SIGN_COUNT = 20;
   ```

2. Create a helper for serial output:
   ```cpp
   static const char* serialLabel(uint16_t offset) {
       return mstr(offset);
   }
   
   // Usage:
   cdc::serial::Console::printf("%s: %s\r\n", serialLabel(STR_SERIAL_USER_ID), status.user_id);
   ```

**Recommendation**: Leave serial command output as-is unless there is a specific requirement for German serial output. Focus i18n efforts on the UI where it matters more for end users.

## References
- `components/mod_gpg/src/GpgModule.cpp:133-179` - Serial command handlers
- `components/serial_cmd/Console.h` - Serial console API
