---
title: "[LOW] I18n string table loads both languages at startup"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The I18n string table is initialized during `ui_init()` (`AppUi.cpp:502`) before it's actually needed. The full string table for both languages is loaded into memory even though only a subset of strings is used during boot.

**Location:** `components/cdc_os_ui/src/AppUi.cpp:502`

## Impact
- **Early memory allocation**: All translation strings allocated before first use
- **Redundant loading**: Both English and German loaded, but only one language is active
- **No lazy loading**: Strings for rarely-used menus (e.g., expert settings) loaded at boot

## Evidence
From `AppUi.cpp:502`:
```cpp
void ui_init(const UiDeps& deps) {
    s_deps = deps;

    // Initialize I18n
    I18n::instance().init();  // Loads all strings!

    // Create LockScreen
    s_lockScreen = new LockScreenView();
    s_lockScreen->init();
    // ...
}
```

The `I18n::init()` loads the entire string table for both languages.

## Recommended Fix
**Lazy-load string tables**:

1. Initialize minimal strings at boot
2. Load full table on first access
3. Cache per-language

```cpp
class I18n {
    bool initialized_ = false;
    Language currentLang_ = Language::EN;
    
    void init() {
        // Load only essential strings for boot
        loadEssentialStrings();
        initialized_ = true;
    }
    
    void ensureLanguage(Language lang) {
        if (!langLoaded_[lang]) {
            loadStringsForLanguage(lang);
            langLoaded_[lang] = true;
        }
    }
    
    String tr(StringId id) {
        ensureLanguage(currentLang_);
        return strings_[currentLang_][id];
    }
};
```

**Alternative**: Load language from NVS before init
```cpp
// Read preferred language from NVS early
Language lang = loadLanguageFromNVS();
I18n::instance().init(lang);  // Only load one language
```

## References
- I18n: [Lazy loading patterns](https://www.i18nwizard.com/)
- ESP32: [Flash access time](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/memory.html) - string tables stored in flash