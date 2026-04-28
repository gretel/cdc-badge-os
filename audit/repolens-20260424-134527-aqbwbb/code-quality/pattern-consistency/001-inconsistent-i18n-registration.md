---
title: "[MEDIUM] Inconsistent i18n string registration pattern across modules"
severity: MEDIUM
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

The module architecture defines a consistent pattern for internationalization (i18n) string registration, but there are **two variations** found across modules:

1. **mod_nvsedit** - Does not register i18n strings at all (uses hardcoded English strings)
2. **mod_ble_serial** - Uses class methods instead of static functions for `registerStrings()` and `mstr()`

**Files affected:**
- `components/mod_nvsedit/src/NvsEditModule.cpp` - Missing i18n registration
- `components/mod_ble_serial/src/BleSerialModule.cpp` - Uses class methods pattern

### Pattern used by most modules (e.g., mod_totp, mod_gpg, mod_fido2, mod_password, mod_hid, mod_vcard, mod_sao)

```cpp
// In TotpModule.cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_TOTP = 0;
static constexpr uint16_t STR_COUNT = 14;

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_totp", STR_COUNT);
    i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::EN, "TOTP");
    i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::DE, "TOTP");
}

// Called in init()
bool TotpModule::init() {
    registerStrings();
    // ...
}
```

### Variation 1: mod_nvsedit (no i18n)

```cpp
// In NvsEditModule.cpp - uses hardcoded strings
static void showDeleteDisabled() {
    showToastError("Delete disabled");  // Hardcoded English
}

// No registerStrings() function
// No mstr() helper
// No i18n registration
```

### Variation 2: mod_ble_serial (class methods)

```cpp
// In BleSerialModule.h
class BleSerialModule : public core::IService {
public:
    void registerStrings();  // Class method, not static
    const char* mstr(uint16_t offset) const;  // Class method
    // ...
};

// In BleSerialModule.cpp
void BleSerialModule::registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_ble_serial", STR_COUNT);
    // ...
}

const char* BleSerialModule::mstr(uint16_t offset) const {
    return ui::tr(s_strIdBase + offset);
}
```

## Impact

1. **Maintenance burden**: Developers need to remember which modules use which pattern
2. **Inconsistent user experience**: German users see mixed English/German UI depending on which module they're using (mod_nvsedit)
3. **Onboarding confusion**: New developers copying patterns may not realize these are exceptions
4. **Future localization**: Adding a third language requires hunting down all hardcoded strings (mod_nvsedit)

## Evidence

**mod_nvsedit (no i18n):**
- `components/mod_nvsedit/src/NvsEditModule.cpp:68` - `showToastError("Delete disabled");`
- No `registerModule()` call
- No `mstr()` helper function
- No `STR_` constants defined

**mod_ble_serial (class methods):**
- `components/mod_ble_serial/src/BleSerialModule.cpp:34` - `void BleSerialModule::registerStrings()`
- `components/mod_ble_serial/src/BleSerialModule.cpp:60` - `const char* BleSerialModule::mstr(uint16_t offset) const`
- `components/mod_ble_serial/include/mod_ble_serial/BleSerialModule.h:24` - `static uint16_t s_strIdBase;` (static member)

**mod_totp (standard pattern):**
- `components/mod_totp/src/TotpModule.cpp:56` - `s_strIdBase = i18n.registerModule("mod_totp", STR_COUNT);`
- `components/mod_totp/src/TotpModule.cpp:47` - `static const char* mstr(uint16_t offset)`
- `components/mod_totp/src/TotpModule.cpp:54` - `static void registerStrings()`

**mod_gpg (standard pattern):**
- `components/mod_gpg/src/GpgModule.cpp:64` - `s_strIdBase = i18n.registerModule("mod_gpg", STR_COUNT);`
- `components/mod_gpg/src/GpgModule.cpp:55` - `static const char* mstr(uint16_t offset)`
- `components/mod_gpg/src/GpgModule.cpp:62` - `static void registerStrings()`

## Recommended Fix

### For mod_nvsedit:

Add i18n string registration following the standard pattern:

1. Add string constants at the top of the file:
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_NVS_EDIT = 0;
static constexpr uint16_t STR_DELETE_DISABLED = 1;
// ... other UI strings
static constexpr uint16_t STR_COUNT = 5;  // Adjust count
```

2. Add `mstr()` helper:
```cpp
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}
```

3. Add `registerStrings()` function:
```cpp
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_nvsedit", STR_COUNT);
    i18n.registerTranslation(s_strIdBase + STR_NVS_EDIT, ui::Language::EN, "NVS Edit");
    i18n.registerTranslation(s_strIdBase + STR_NVS_EDIT, ui::Language::DE, "NVS Bearbeiten");
    i18n.registerTranslation(s_strIdBase + STR_DELETE_DISABLED, ui::Language::EN, "Delete disabled");
    i18n.registerTranslation(s_strIdBase + STR_DELETE_DISABLED, ui::Language::DE, "Loeschen deaktiviert");
}
```

4. Call `registerStrings()` in `init()` method
5. Replace all hardcoded strings with `mstr()` calls

### For mod_ble_serial:

Consider refactoring to use static functions for consistency:

```cpp
// Change from:
void BleSerialModule::registerStrings() { ... }
const char* BleSerialModule::mstr(uint16_t offset) const { ... }

// To:
static void registerStrings() { ... }
static const char* mstr(uint16_t offset) { ... }
```

This removes the dependency on `this` pointer and makes the pattern consistent with other modules.

## References

- CLAUDE.md: Module Architecture section describes self-contained modules with i18n registration
- `components/mod_totp/src/TotpModule.cpp` - Reference implementation
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n API documentation
