---
title: "[MEDIUM] I18n singleton uses static arrays for translation storage with no module state isolation"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The `I18n` singleton (`components/cdc_ui/include/cdc_ui/I18n.h`) uses static 2D arrays (`strings_[LANG_COUNT][MAX_STRINGS]`) to store translations. Modules register strings dynamically via `registerModule()` but there is no isolation between modules - a module can accidentally overwrite another module's strings if the base ID calculation is wrong.

### Evidence:
- `I18n.h:281-285`: Static arrays `strings_[LANG_COUNT][MAX_STRINGS]`, `nextModuleId_`
- `I18n.h:252-260`: `registerModule()` reserves IDs but returns base ID with no validation
- `I18n.h:262-270`: `registerTranslation()` has no bounds checking or duplicate detection
- `I18n.h:276`: `getStringCount()` returns total but no way to query per-module counts

### State Access Pattern:
```cpp
// Module registers for N strings
uint16_t baseId = i18n.registerModule("totp", 50);

// Module registers translations
i18n.registerTranslation(baseId + 0, Language::EN, "TOTP Codes");
i18n.registerTranslation(baseId + 0, Language::DE, "TOTP Codes");

// But no validation:
// - What if baseId + 50 exceeds MAX_STRINGS?
// - What if two modules register same baseId?
// - What if module forgets to register all 50 strings?
```

## Impact
1. **String collision**: Module B can overwrite Module A's strings if base ID calculation is wrong
2. **No bounds validation**: `registerTranslation(baseId + 100)` can exceed `MAX_STRINGS = 512`
3. **Memory waste**: Module reserves 50 IDs but only uses 10 - no way to reclaim
4. **Debugging difficulty**: Can't query which module owns which string ID
5. **No duplicate detection**: `registerTranslation()` called twice with same ID overwrites silently

## Recommended Fix
1. Add module isolation:
   ```cpp
   typedef struct {
       const char* moduleName;
       uint16_t baseId;
       uint16_t count;
   } ModuleStringInfo;
   
   ModuleStringInfo getModuleInfo(const char* moduleName) const;
   ```

2. Add bounds validation:
   ```cpp
   bool registerTranslation(uint16_t stringId, Language lang, const char* text) {
       if (stringId >= MAX_STRINGS) {
           LOG_W("I18n", "String ID %u exceeds MAX_STRINGS", stringId);
           return false;
       }
       // Check for duplicate
       if (strings_[static_cast<int>(lang)][stringId]) {
           LOG_W("I18n", "Duplicate translation for ID %u", stringId);
       }
       // ... rest of logic
   }
   ```

3. Add string query API:
   ```cpp
   typedef struct {
       uint16_t stringId;
       Language lang;
       const char* text;
       const char* ownerModule;
   } StringInfo;
   
   StringInfo getStringInfo(uint16_t id) const;
   ```

4. Add string count per module:
   ```cpp
   uint16_t getModuleStringCount(const char* moduleName) const;
   ```

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - Full header
- `components/cdc_ui/src/I18n.cpp` - Implementation
- `components/mod_fido2/src/Fido2Ui.cpp` - Module string registration usage
