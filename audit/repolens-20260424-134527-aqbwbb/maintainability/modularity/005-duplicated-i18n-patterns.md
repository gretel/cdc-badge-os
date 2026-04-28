---
title: "[MEDIUM] Duplicated i18n registration patterns across modules"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
Every module (`mod_gpg`, `mod_password`, `mod_totp`, `grove_led`) implements its own i18n string registration with nearly identical code patterns: a `registerStrings()` function, a `mstr()` helper, string offset constants, and English/German registration calls. This duplication should be extracted into a reusable module-level helper.

**Evidence:**
- `mod_gpg/src/GpgModule.cpp` (lines 28-85): i18n registration
- `mod_password/src/PasswordModule.cpp` (lines 27-75): i18n registration
- `mod_totp/src/TotpModule.cpp` (lines 22-65): i18n registration
- `grove_led/src/GroveLedModule.cpp` (lines 18-90): i18n registration

## Impact
1. **Maintenance burden**: Adding a new language requires updating 10+ files with the same pattern
2. **Error-prone**: Developers may forget to register strings for a language or use wrong offset
3. **Inconsistency**: Each module implements slightly different variations
4. **Code size**: ~60 lines of nearly identical code repeated 10+ times = 600+ lines of duplication
5. **Discoverability**: New developers don't know where to look for the pattern

## Evidence
**mod_gpg/src/GpgModule.cpp:**
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_GPG = 0;
static constexpr uint16_t STR_STATUS = 1;
static constexpr uint16_t STR_GENERATE = 2;
// ... more constants ...
static constexpr uint16_t STR_COUNT = 17;

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_gpg", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }
    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::EN, "GPG");
    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::DE, "GPG");
    // ... more registrations ...
}
```

**mod_password/src/PasswordModule.cpp:**
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_PASSWORDS = 0;
static constexpr uint16_t STR_NEW_ENTRY = 1;
static constexpr uint16_t STR_TITLE = 2;
// ... more constants ...
static constexpr uint16_t STR_COUNT = 21;

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_password", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }
    i18n.registerTranslation(s_strIdBase + STR_PASSWORDS, ui::Language::EN, "Passwords");
    i18n.registerTranslation(s_strIdBase + STR_PASSWORDS, ui::Language::DE, "Passwörter");
    // ... more registrations ...
}
```

**Identical pattern in 4+ modules:**
- `mod_gpg/src/GpgModule.cpp`
- `mod_password/src/PasswordModule.cpp`
- `mod_totp/src/TotpModule.cpp`
- `grove_led/src/GroveLedModule.cpp`

## Recommended Fix
**Option A: Create i18n module helper (Recommended)**
1. Create `components/cdc_ui/src/I18nHelpers.h` with `ModuleStrings` class (20 minutes)
   ```cpp
   namespace cdc::ui {
   class ModuleStrings {
   public:
       ModuleStrings(const char* moduleName, uint16_t count);
       const char* get(uint16_t offset) const;
       template<typename T> void registerEnglish(T offset, const char* en);
       template<typename T> void registerGerman(T offset, const char* de);
   private:
       uint16_t baseId_;
   };
   }
   ```
2. Refactor `mod_gpg` to use new helper (10 minutes)
3. Refactor `mod_password` to use new helper (10 minutes)
4. Refactor `mod_totp` to use new helper (10 minutes)
5. Refactor `grove_led` to use new helper (10 minutes)

**Option B: Use C++20 consteval for compile-time registration (Advanced)**
- More complex but eliminates runtime overhead

**Total estimated time: ~60 minutes for Option A**

## References
- DRY principle: https://en.wikipedia.org/wiki/Don%27t_repeat_yourself
- Existing `cdc_ui/I18n.h` provides base functionality
- Similar to `RenderHelpers` pattern for reusable view utilities
