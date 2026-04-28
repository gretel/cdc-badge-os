---
title: "[LOW] String table initialization runs synchronously at startup"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
Internationalization (i18n) strings are registered **synchronously** during module initialization. Each module registers all its translations in a loop, which can add up across 10+ modules.

**Location:** `components/mod_totp/src/TotpModule.cpp:62-91`, similar patterns in all modules

## Impact
- **Cumulative delay**: 14 strings × 10 modules = 140+ `registerTranslation()` calls
- **Blocking operation**: Each call iterates the string table
- **Memory allocation**: Dynamic string storage for each translation

## Evidence
From `components/mod_totp/src/TotpModule.cpp:62-91`:
```cpp
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_totp", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::EN, "TOTP");
    i18n.registerTranslation(s_strIdBase + STR_ADD_ACCOUNT, ui::Language::EN, "Add Account");
    // ... 12 more for English ...
    
    i18n.registerTranslation(s_strIdBase + STR_TOTP, ui::Language::DE, "TOTP");
    i18n.registerTranslation(s_strIdBase + STR_ADD_ACCOUNT, ui::Language::DE, "Account hinzufuegen");
    // ... 12 more for German ...
    
    LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
}
```

Called from `TotpModule::init()`:
```cpp
bool TotpModule::init() {
    LOG_I(TAG, "Initializing TOTP module");
    registerStrings();  // ~28 function calls
    registerCommands();
    // ...
}
```

With 10 modules averaging 20 strings each:
- 10 × 20 = 200 `registerTranslation()` calls
- Each call: string copy + table insertion
- Estimated: 200 × 0.1ms = 20ms total

## Recommended Fix
**Option 1**: Static string tables with batch registration
```cpp
static const ui::I18n::Translation s_totpStrings[] = {
    {ui::Language::EN, "TOTP"},
    {ui::Language::EN, "Add Account"},
    // ...
};

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_totp", STR_COUNT);
    i18n.registerTranslations(s_strIdBase, s_totpStrings, STR_COUNT);  // Batch
}
```

**Option 2**: Deferred registration
```cpp
// Register only on first use
static bool s_stringsRegistered = false;
static void ensureStringsRegistered() {
    if (!s_stringsRegistered) {
        registerStrings();
        s_stringsRegistered = true;
    }
}

// Call from getMenuItems() instead of init()
```

**Option 3**: Compile-time string generation
```cpp
// Generate C header at build time
// strings_totp_en.h, strings_totp_de.h
// Pre-populate table in .rodata, no runtime registration
```

## References
- ESP32 ROM: [String storage in flash](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/memory/mem_alloc.html) - Use IRAM_ATTR for hot paths
