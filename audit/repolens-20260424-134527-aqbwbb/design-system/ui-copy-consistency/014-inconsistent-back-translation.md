---
title: "[LOW] Inconsistent use of "Back" vs "Zurueck" in footer hints"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "i18n"
  - "navigation"
---

## Summary
Footer hints use inconsistent patterns for the "Back" action in English, and the German translations show similar inconsistency:

1. **English footer hints use "Back" consistently:**
   - `[N] Back` (HINT_BACK)
   - `[Y] OK [N] Back` (HINT_OK_BACK)
   - `[N] Back` (HINT_SCROLL_BACK)
   - `[3] Edit  [N] Back` (mod_totp STR_HINT_EDIT)
   - `[Y] Type  [2/8] Scroll  [N] Back` (mod_password STR_HINT_TYPE)

2. **German footer hints use inconsistent translations:**
   - `Zuruck` (HINT_BACK, HINT_OK_BACK, HINT_SCROLL_BACK)
   - `Zurueck` (mod_totp STR_HINT_EDIT, mod_password STR_HINT_TYPE)
   - Different spellings for the same word!

3. **Inconsistent German translation patterns:**
   - "Zuruck" (without umlaut expansion) in core strings
   - "Zurueck" (with ue expansion) in module strings
   - Both mean "Zurück" in proper German

## Impact
- **German users see inconsistent spelling**: "Zuruck" in some places, "Zurueck" in others
- **Confusion**: Same action spelled differently
- **Translation quality**: Both are technically acceptable but should be consistent
- **Professionalism**: Inconsistent spelling reduces polish

## Evidence
**Core strings (components/cdc_ui/src/I18n.cpp):**

**File: `components/cdc_ui/src/I18n.cpp` lines 218, 352-353, 360**
```cpp
REG(BACK,           "Back",             "Zuruck");
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
REG(HINT_SCROLL_BACK,   "[2/8] Scroll [N] Back","[2/8] Scrollen [N] Zuruck");
```

**Module strings (mod_totp):**

**File: `components/mod_totp/src/TotpModule.cpp` lines 88-89**
```cpp
i18n.registerTranslation(s_strIdBase + STR_HINT_EDIT, ui::Language::DE, "[3] Edit  [N] Zurueck");
i18n.registerTranslation(s_strIdBase + STR_HINT_TYPE, ui::Language::DE, "[Y] Tippen  [3] Edit  [N] Zurueck");
```

**Module strings (mod_password):**

**File: `components/mod_password/src/PasswordModule.cpp` line 111**
```cpp
i18n.registerTranslation(s_strIdBase + STR_HINT_LIST, ui::Language::DE, "[Y] Ansehen  [3] Menu  [N] Zurueck");
```

**Note the inconsistency:**
- Core: `Zuruck`
- Modules: `Zurueck`

## Recommended Fix
1. **Choose one convention and apply consistently:**
   - Option A (Recommended): Use `Zurueck` (more explicit ue expansion)
   - Option B: Use `Zuruck` (shorter, already established in core)

2. **Update core strings if choosing Option A:**
   ```cpp
   // In components/cdc_ui/src/I18n.cpp
   REG(BACK,           "Back",             "Zurueck");
   REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zurueck");
   REG(HINT_SCROLL_BACK,   "[2/8] Scroll [N] Back","[2/8] Scrollen [N] Zurueck");
   ```

3. **Or update module strings if choosing Option B:**
   ```cpp
   // In components/mod_totp/src/TotpModule.cpp
   i18n.registerTranslation(s_strIdBase + STR_HINT_EDIT, ui::Language::DE, "[3] Edit  [N] Zuruck");
   i18n.registerTranslation(s_strIdBase + STR_HINT_TYPE, ui::Language::DE, "[Y] Tippen  [3] Edit  [N] Zuruck");
   
   // In components/mod_password/src/PasswordModule.cpp
   i18n.registerTranslation(s_strIdBase + STR_HINT_LIST, ui::Language::DE, "[Y] Ansehen  [3] Menu  [N] Zuruck");
   ```

4. **Audit for similar inconsistencies:**
   - Check all German translations for spelling consistency
   - Common patterns: "ae" vs "a", "oe" vs "o", "ue" vs "u"

5. **Document the convention:**
   - Use full umlaut expansion (ae, oe, ue) consistently
   - Apply to all new translations

## References
- Internationalization: German umlaut handling
- UI Copy Consistency: Translation consistency
- German language localization guidelines
