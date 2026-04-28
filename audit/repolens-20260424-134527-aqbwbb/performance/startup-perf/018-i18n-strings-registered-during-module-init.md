---
title: "[LOW] I18n String Tables Registered Sequentially During Module Init"
severity: LOW
domain: startup-perf
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
I18n (internationalization) string tables for each module are registered sequentially during the module initialization phase in `main.cpp:224-227`. Each module's `registerStrings()` function is called one after another, and each registration involves string operations and hash table lookups. While individual registrations are fast (~1-2ms), the cumulative effect across 10 modules adds up.

**Evidence:**
```cpp
// main/main.cpp:224-227
modules_register_all();  // Registers all initializers
ModuleRegistry::instance().runAllInitializers();  // Calls each module's init()
```

In `components/mod_gpg/src/GpgModule.cpp:54-105`, `registerStrings()` registers ~17 English and 17 German strings (34 total `registerTranslation()` calls).

In `components/mod_totp/src/TotpModule.cpp:52-91`, `registerStrings()` registers ~14 English and 14 German strings (28 total calls).

## Impact
- **Cumulative Overhead**: 10 modules × ~30 strings each × ~1ms per registration = ~300ms total
- **Sequential Processing**: No parallelization; strings registered one module at a time
- **Memory Fragmentation**: Multiple small allocations during boot
- **Redundant Work**: Strings are registered every boot, could be cached

## Evidence
File: `components/mod_gpg/src/GpgModule.cpp` lines 54-105
```cpp
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_gpg", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::EN, "GPG");
    i18n.registerTranslation(s_strIdBase + STR_STATUS, ui::Language::EN, "Status");
    // ... 32 more registerTranslation() calls ...
}
```

Similar pattern in `mod_totp` (lines 52-91) and all other modules.

## Recommended Fix
1. **Pre-compile string tables into binary format:**
   ```cpp
   // Generate at build time:
   // components/mod_gpg/i18n_en.bin, i18n_de.bin
   
   // Load at boot (faster than individual registrations):
   static void loadStrings() {
       auto& i18n = ui::I18n::instance();
       i18n.loadModule("mod_gpg", i18n_en_bin, i18n_de_bin);
   }
   ```

2. **Cache string tables in NVS after first registration:**
   ```cpp
   // First boot: register strings normally
   // Subsequent boots: load from NVS cache
   bool loadFromCache() {
       nvs_get_blob(..., &cachedStrings, &size);
       return true;
   }
   ```

3. **Parallel registration using tasks:**
   ```cpp
   // Create tasks for each module's string registration
   xTaskCreate([](void* arg) {
       registerStringsForModule((ModuleType)arg);
   }, ...);
   ```

4. **Minimal optimization: defer non-critical strings**
   - Only register strings needed for boot splash and main menu
   - Defer rest until module view is first opened

The simplest fix (option 1) can reduce string registration time by 50-70%.

## References
- ESP32 memory allocation can fragment with many small allocations
- String hashing and table lookups add up across 300+ registrations
- Similar optimization already used for slot map (compile-time generation)
