---
title: "[LOW] Hardcoded string IDs in UI core"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
String IDs for UI text are defined as a fixed enum in the I18n system. Adding new UI strings requires modifying the core `StringId` enum, which violates the Open/Closed Principle.

**Evidence:**
- `components/cdc_ui/` - I18n system with hardcoded StringId enum

## Impact
- **Extension Barrier**: New UI strings require editing core code
- **Module Coupling**: Modules must depend on core StringId enum
- **Scalability**: As UI grows, the enum becomes unwieldy

## Evidence
The I18n system uses `ui::tr(StringId::...)` pattern in `components/cdc_os_ui/src/AppUi.cpp:328-380`:
```cpp
s_mainMenuItems[getToolsIndex()] = {tr(StringId::TOOLS), 0, false, nullptr};
s_mainMenuItems[getSettingsIndex()] = {tr(StringId::SETTINGS), 0, false, nullptr};
// ...
s_settingsItems[SETTINGS_IDX_BRIGHTNESS] = {tr(StringId::BRIGHTNESS), 0, false, nullptr};
s_settingsItems[SETTINGS_IDX_LANGUAGE] = {tr(StringId::LANGUAGE), 0, false, nullptr};
```

## Recommended Fix
Implement dynamic string registration for modules:

1. **Keep core strings in enum, add module registration**:
   ```cpp
   class I18n {
   public:
       // Core strings (enum-based)
       static const char* tr(StringId id);
       
       // Module strings (dynamic)
       uint16_t registerModuleString(const char* key);
       const char* tr(uint16_t stringId);
       
       // Set current language
       void setLanguage(Language lang);
   };
   ```

2. **Modules register their strings**:
   ```cpp
   // In mod_totp
   void TotpModule::init() {
       // Register strings
       s_stringIds.issuer = I18n::instance().registerModuleString("TOTP_ISSUER");
       s_stringIds.secret = I18n::instance().registerModuleString("TOTP_SECRET");
       s_stringIds.digits = I18n::instance().registerModuleString("TOTP_DIGITS");
   }
   ```

3. **String tables per language**:
   ```cpp
   // Core strings (existing)
   static const char* coreStrings[] = {
       "Tools", "Settings", "Brightness", // English
       "Werkzeuge", "Einstellungen", "Helligkeit", // German
   };
   
   // Module strings (dynamic)
   struct ModuleStringTable {
       const char* key;
       const char* en;
       const char* de;
   };
   
   static std::vector<ModuleStringTable> moduleStrings;
   ```

This allows modules to add their own translatable strings without editing core code.

## References
- Internationalization: Externalize all UI text
- Plugin Architecture: Dynamic extension points
- Resource Files: Separate data from code
