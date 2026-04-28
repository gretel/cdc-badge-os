---
title: "[LOW] i18n string registration mixed with module business logic"
severity: LOW
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
Modules register their i18n strings inline within their initialization logic, mixing internationalization concerns with core business logic. The `registerStrings()` function in both TOTP and Password modules is called from `init()` and handles both string registration and business initialization.

**Location**: 
- `components/mod_totp/src/TotpModule.cpp:48-91`
- `components/mod_password/src/PasswordModule.cpp:57-114`

## Impact
- **Initialization coupling**: i18n strings must be registered before any business logic can run
- **Hard to extract**: Internationalization cannot be separated for different locales
- **Testing complexity**: Even unit tests need i18n system initialized

## Evidence
```cpp
// TotpModule.cpp:91-93
bool TotpModule::init() {
    LOG_I(TAG, "Initializing TOTP module");
    registerStrings();  // i18n mixed with business init
    
    // ... more business logic ...
}

// registerStrings() function (lines 48-91)
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_totp", STR_COUNT);
    // ... 40+ lines of string registration ...
}
```

## Recommended Fix
1. **Create a dedicated module** for i18n registration:
   ```cpp
   namespace cdc::mod_totp {
       class TotpI18n {
       public:
           static void registerAll();
           static const char* str(uint16_t offset);
       };
   }
   ```

2. **Call i18n registration separately** during system boot:
   ```cpp
   // In main.cpp or dedicated i18n_init()
   mod_totp::TotpI18n::registerAll();
   mod_password::PasswordI18n::registerAll();
   ```

3. **Module init()** only handles business logic:
   ```cpp
   bool TotpModule::init() {
       // Pure business logic, no i18n concerns
       TotpStore::instance().setSlotRange(...);
       return true;
   }
   ```

## References
- [Internationalization and localization](https://en.wikipedia.org/wiki/Internationalization_and_localization)
- [Separation of concerns](https://en.wikipedia.org/wiki/Separation_of_concerns)
