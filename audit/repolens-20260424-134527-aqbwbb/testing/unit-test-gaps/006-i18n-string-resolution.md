---
title: "[MEDIUM] I18n String Resolution and Module Registration Lacks Unit Test Coverage"
severity: MEDIUM
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `I18n` class (`components/cdc_ui/src/I18n.cpp`, 372 lines) provides internationalization for the UI with no unit tests. Critical untested functions include:

- `init()` (line 41) - Core string initialization and language load
- `str(uint16_t)` (line 92) - String resolution with fallback
- `registerModule()` (line 121) - Module ID reservation
- `registerTranslation()` (line 142) - Single translation registration
- `registerTranslations()` (line 157) - Batch registration
- `setLanguage()` (line 57) - Language switch with NVS persist
- `loadFromNvs()` (line 168) - Language load from NVS
- `saveToNvs()` (line 183) - Language save to NVS

## Impact
**UI Localization Risk:** I18n is used by all UI components:
1. String resolution fallback (current lang → English → "?") is untested
2. Module ID reservation can overflow MAX_STRINGS (512) - untested
3. Batch registration with 0xFFFF terminator is unproven
4. Language NVS persistence with invalid values needs testing
5. `initCoreStrings()` has 100+ REG() macros - potential typos

## Evidence
File: `components/cdc_ui/src/I18n.cpp`

Line 92-109: `str(uint16_t)` - Fallback logic
```cpp
const char* I18n::str(uint16_t id) const {
    if (id >= MAX_STRINGS) {
        return "?";  // Overflow
    }

    const char* text = strings_[static_cast<uint8_t>(currentLang_)][id];
    if (text) {
        return text;
    }

    const char* text = strings_[static_cast<uint8_t>(Language::EN)][id];
    if (text) {
        return text;  // Fallback to English
    }

    return "?";  // No translation found
}
```

Line 121-134: `registerModule()` - ID reservation
```cpp
uint16_t I18n::registerModule(const char* moduleName, uint16_t count) {
    if (nextModuleId_ + count > MAX_STRINGS) {
        LOG_E(TAG, "Cannot register module '%s': out of string slots", moduleName);
        return 0;  // Overflow
    }

    uint16_t baseId = nextModuleId_;
    nextModuleId_ += count;
    return baseId;
}
```

Line 157-164: `registerTranslations()` - Batch registration
```cpp
void I18n::registerTranslations(const Translation* translations) {
    if (!translations) return;

    while (translations->stringId != 0xFFFF) {  // Terminator
        registerTranslation(translations->stringId, translations->lang, translations->text);
        translations++;
    }
}
```

Line 57-69: `setLanguage()` - Persistence
```cpp
void I18n::setLanguage(Language lang) {
    if (lang >= Language::COUNT) {
        lang = Language::EN;  // Clamp invalid
    }
    if (currentLang_ != lang) {
        currentLang_ = lang;
        saveToNvs();  // Persist
    }
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "I18n" {} \;
# Returns nothing - no I18n tests exist
```

## Recommended Fix
Create `test/test_i18n/test_i18n.cpp` with test cases:

1. **String resolution tests:**
   - Test `str()` returns current language string
   - Test `str()` returns English fallback when current lang missing
   - Test `str()` returns "?" when no translation exists
   - Test `str()` with overflow ID

2. **Module registration tests:**
   - Test `registerModule()` returns sequential base IDs
   - Test `registerModule()` returns 0 on overflow
   - Test `registerTranslation()` after `registerModule()`

3. **Batch registration tests:**
   - Test `registerTranslations()` with valid array
   - Test `registerTranslations()` with 0xFFFF terminator
   - Test `registerTranslations()` with nullptr

4. **Language tests:**
   - Test `setLanguage()` persists to NVS
   - Test `loadFromNvs()` loads persisted language
   - Test `setLanguage()` with invalid value clamps to English

Example test:
```cpp
void test_str_fallback_to_english() {
    auto& i18n = I18n::instance();
    i18n.init();
    
    // Set language to German
    i18n.setLanguage(Language::DE);
    
    // German has translation
    const char* text = i18n.str(StringId::SETTINGS);
    TEST_ASSERT_EQUAL_STRING("Einstellungen", text);
    
    // Register module-specific string (German only)
    uint16_t baseId = i18n.registerModule("test", 10);
    i18n.registerTranslation(baseId + 0, Language::DE, "Test DE");
    
    // German should return DE translation
    text = i18n.str(baseId + 0);
    TEST_ASSERT_EQUAL_STRING("Test DE", text);
    
    // English fallback should return "?"
    i18n.setLanguage(Language::EN);
    text = i18n.str(baseId + 0);
    TEST_ASSERT_EQUAL_STRING("?", text);
}

void test_registerModule_overflow() {
    auto& i18n = I18n::instance();
    i18n.init();
    
    // Reserve most strings
    uint16_t baseId = i18n.registerModule("test1", 400);
    TEST_ASSERT_GREATER_THAN(0, baseId);
    
    // Should succeed
    baseId = i18n.registerModule("test2", 100);
    TEST_ASSERT_GREATER_THAN(0, baseId);
    
    // Should fail (overflow)
    baseId = i18n.registerModule("test3", 50);
    TEST_ASSERT_EQUAL(0, baseId);
}

void test_registerTranslations_terminator() {
    auto& i18n = I18n::instance();
    i18n.init();
    
    uint16_t baseId = i18n.registerModule("test", 5);
    
    Translation translations[] = {
        {baseId + 0, Language::EN, "String 0"},
        {baseId + 0, Language::DE, "Zeichenkette 0"},
        {baseId + 1, Language::EN, "String 1"},
        {0xFFFF, Language::EN, "End"}  // Terminator
    };
    
    i18n.registerTranslations(translations);
    
    TEST_ASSERT_EQUAL_STRING("String 0", i18n.str(baseId + 0));
    TEST_ASSERT_EQUAL_STRING("Zeichenkette 0", i18n.str(baseId + 0));
    TEST_ASSERT_EQUAL_STRING("String 1", i18n.str(baseId + 1));
}
```

## References
- File: `components/cdc_ui/include/cdc_ui/I18n.h` - Full API and StringId enum
- Pattern: String table with language fallback
