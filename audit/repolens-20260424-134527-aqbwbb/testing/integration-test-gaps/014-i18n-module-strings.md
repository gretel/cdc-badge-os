---
title: "[HIGH] I18n dynamic module string registration lacks integration tests"
severity: HIGH
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_ui"
  - "area:i18n"
---

## Summary
The `I18n` component (`components/cdc_ui/include/cdc_ui/I18n.h`) supports dynamic module string registration for internationalization, but **no integration tests** verify that module strings are registered correctly and retrieved with proper language switching.

## Impact
- **String registration failures**: Module strings may not be registered in correct order
- **Language switching**: German translations may not appear correctly
- **String ID collisions**: Modules may overwrite each other's strings
- **Fallback behavior**: Missing translations may not fall back to English

## Evidence

**I18n API** (`components/cdc_ui/include/cdc_ui/I18n.h:200-280`):
```cpp
class I18n {
    uint16_t registerModule(const char* moduleName, uint16_t count);
    void registerTranslation(uint16_t stringId, Language lang, const char* text);
    const char* tr(StringId id);
    void setLanguage(Language lang);
    Language getLanguage() const;
};
```

**Module registration pattern** (`components/mod_gpg/src/GpgModule.cpp:50-90`):
```cpp
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_gpg", STR_COUNT);
    
    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::EN, "GPG");
    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::DE, "GPG");
}
```

**String retrieval** (`components/mod_fido2/src/Fido2Ui.cpp:48-50`):
```cpp
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}
```

**Modules registering strings**:
- `mod_gpg` - 8 strings
- `mod_fido2` - 8 strings
- `mod_totp` - 14 strings
- `mod_password` - 21 strings
- `mod_vcard` - strings for vCard display

**Current test coverage**: None

## Recommended Fix

Create integration test `test_i18n_integration/` that verifies:

1. **Module registration**: `registerModule()` returns unique base ID
2. **Translation storage**: All translations stored correctly
3. **String retrieval**: `tr()` returns correct string for ID
4. **Language switching**: `setLanguage()` changes active language
5. **Fallback behavior**: Missing translation returns English
6. **Multiple modules**: Multiple modules can register simultaneously

**Test structure** (example):
```cpp
// test/test_i18n_integration/test_i18n.cpp
#include "cdc_ui/I18n.h"

void test_module_registration() {
    auto& i18n = ui::I18n::instance();
    i18n.init();
    
    uint16_t base1 = i18n.registerModule("mod_gpg", 10);
    uint16_t base2 = i18n.registerModule("mod_fido2", 10);
    
    ASSERT_EQ(base1, 0);
    ASSERT_EQ(base2, 10);
}

void test_language_switching() {
    auto& i18n = ui::I18n::instance();
    i18n.init();
    
    uint16_t base = i18n.registerModule("test", 1);
    i18n.registerTranslation(base, ui::Language::EN, "English");
    i18n.registerTranslation(base, ui::Language::DE, "German");
    
    i18n.setLanguage(ui::Language::EN);
    ASSERT_STREQ(i18n.tr(base), "English");
    
    i18n.setLanguage(ui::Language::DE);
    ASSERT_STREQ(i18n.tr(base), "German");
}

void test_missing_translation_fallback() {
    auto& i18n = ui::I18n::instance();
    i18n.init();
    
    uint16_t base = i18n.registerModule("test", 1);
    i18n.registerTranslation(base, ui::Language::EN, "English");
    // No German translation
    
    i18n.setLanguage(ui::Language::DE);
    // Should fall back to English
    ASSERT_STREQ(i18n.tr(base), "English");
}
```

## References
- [I18n header](components/cdc_ui/include/cdc_ui/I18n.h)
- [I18n implementation](components/cdc_ui/src/I18n.cpp)
- [Module registration example](components/mod_fido2/src/Fido2Ui.cpp:55-82)

</content>