---
title: "[LOW] Inconsistent module string registration patterns"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "i18n"
  - "code-consistency"
---

## Summary
Module i18n string registration follows different patterns across modules, making the codebase less maintainable:

1. **Different registration function names:**
   - `registerStrings()` - mod_password, mod_totp, mod_vcard, mod_hid
   - `BleSerialModule::registerStrings()` - mod_ble_serial (member function)
   - `registerModule()` - called from main registration (different context)

2. **Different string ID offset patterns:**
   - `static constexpr uint16_t STR_PASSWORDS = 0;` - mod_password
   - `static uint16_t s_strIdBase = 0;` followed by `static constexpr` - mod_totp
   - `uint16_t BleSerialModule::s_strIdBase = 0;` - mod_ble_serial (class member)

3. **Different mstr() helper patterns:**
   - `static const char* mstr(uint16_t offset)` - mod_password, mod_totp, mod_vcard, mod_hid
   - `const char* BleSerialModule::mstr(uint16_t offset) const` - mod_ble_serial (member function)

4. **Different English/German registration order:**
   - mod_password: All English first, then all German
   - mod_totp: All English first, then all German
   - mod_vcard: All English first, then all German
   - mod_ble_serial: All English first, then all German
   - mod_hid: All English first, then all German

5. **Different logging patterns:**
   - `LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);` - mod_password, mod_totp, mod_ble_serial
   - `LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);` - mod_hid
   - No logging - mod_vcard

## Impact
- **Maintenance burden**: New developers need to learn multiple patterns
- **Code review complexity**: Inconsistent patterns make review harder
- **Copy-paste errors**: Developers may copy wrong pattern
- **Reduced readability**: Consistent patterns improve scanability

## Evidence
**File: `components/mod_password/src/PasswordModule.cpp` lines 28-50**
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_PASSWORDS = 0;
static constexpr uint16_t STR_NEW_ENTRY = 1;
// ...

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}
```

**File: `components/mod_totp/src/TotpModule.cpp` lines 25-48**
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_TOTP = 0;
static constexpr uint16_t STR_ADD_ACCOUNT = 1;
// ...

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}
```

**File: `components/mod_ble_serial/src/BleSerialModule.cpp` lines 22-60**
```cpp
uint16_t BleSerialModule::s_strIdBase = 0;

// Member function instead of static helper
const char* BleSerialModule::mstr(uint16_t offset) const {
    return ui::tr(s_strIdBase + offset);
}
```

**File: `components/mod_vcard/src/VcardModule.cpp` lines 24-49**
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_VCARD = 0;
// ...
// No logging after registration
```

## Recommended Fix
1. **Establish standard pattern for module i18n:**
   ```cpp
   // Standard pattern:
   static uint16_t s_strIdBase = 0;
   static constexpr uint16_t STR_FIRST_STRING = 0;
   static constexpr uint16_t STR_SECOND_STRING = 1;
   // ...
   static constexpr uint16_t STR_COUNT = N;
   
   static const char* mstr(uint16_t offset) {
       return ui::tr(s_strIdBase + offset);
   }
   ```

2. **Standardize registration order:**
   - All English translations first
   - Then all German translations
   - Keep this pattern consistent

3. **Add logging to all modules:**
   ```cpp
   LOG_I(TAG, "Registered i18n strings (base=%d, count=%d)", s_strIdBase, STR_COUNT);
   ```

4. **Update mod_vcard to add logging:**
   ```cpp
   // Add at end of registerStrings():
   LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
   ```

5. **Document the pattern:**
   - Add example to MODULE_DEVELOPMENT.md
   - Include i18n registration template

## References
- Module Architecture guidelines
- Code consistency best practices
- i18n registration patterns
