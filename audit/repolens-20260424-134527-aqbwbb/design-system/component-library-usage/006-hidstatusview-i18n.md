---
title: "[MEDIUM] HidStatusView has hardcoded English strings, bypassing I18n system"
severity: MEDIUM
domain: component-library-usage
lens: ui-components
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The `HidStatusView` class in the `mod_hid` module has multiple hardcoded English strings that bypass the project's I18n system, while the same module correctly uses I18n for other text.

**Location:**
- `components/mod_hid/src/HidModule.cpp:142,145,148` - Hardcoded OS names "Windows", "Linux", "macOS"
- `components/mod_hid/src/HidModule.cpp:170` - Hardcoded footer hint "[N] Back"

## Impact
1. **Language Inconsistency**: The module registers German translations for its strings (lines 68-74) but the `HidStatusView` class ignores them and displays English-only text.

2. **User Experience**: German users will see a mixed UI:
   - Menu items in German (e.g., "BLE Tastatur", "Status")
   - Status view with English: "Windows", "Linux", "macOS", "[N] Back"

3. **Pattern Violation**: The module correctly uses `mstr()` helper for all menu items (lines 211-218) but breaks the pattern in the status view.

4. **Maintainability**: If OS names need to be changed or additional languages added, developers may miss these hardcoded strings.

## Evidence
**HidModule.cpp (lines 142-148 - hardcoded OS names):**
```cpp
case UnicodeMethod::WINDOWS:
    gfx->print("Windows");  // Should use I18n
    break;
case UnicodeMethod::LINUX:
    gfx->print("Linux");    // Should use I18n
    break;
case UnicodeMethod::MACOS:
    gfx->print("macOS");    // Should use I18n
    break;
```

**Same file, correct I18n usage (lines 211-218):**
```cpp
s_menuItems[MENU_STATUS] = {mstr(STR_STATUS), 0, false, nullptr};
s_menuItems[MENU_TOGGLE_ADV] = {mstr(STR_START_ADV), 0, false, nullptr};
s_menuItems[MENU_UNICODE] = {mstr(STR_UNICODE_METHOD), 0, false, nullptr};
```

**Footer hint (line 170 - hardcoded English):**
```cpp
const char* getFooterHint() const override { return "[N] Back"; }
```

**Module already has German translations registered (lines 68-74):**
```cpp
i18n.registerTranslation(s_strIdBase + STR_WINDOWS, ui::Language::DE, "Windows (Alt+Numpad)");
i18n.registerTranslation(s_strIdBase + STR_LINUX, ui::Language::DE, "Linux (Ctrl+Shift+U)");
i18n.registerTranslation(s_strIdBase + STR_MACOS, ui::Language::DE, "macOS (eingeschraenkt)");
```

## Recommended Fix
**1. Add OS name strings to I18n registration:**

Add new string IDs after line 32:
```cpp
static constexpr uint16_t STR_OS_WINDOWS = 10;
static constexpr uint16_t STR_OS_LINUX = 11;
static constexpr uint16_t STR_OS_MACOS = 12;
static constexpr uint16_t STR_COUNT = 13;  // Update count
```

Register English and German translations (after line 74):
```cpp
i18n.registerTranslation(s_strIdBase + STR_OS_WINDOWS, ui::Language::EN, "Windows");
i18n.registerTranslation(s_strIdBase + STR_OS_LINUX, ui::Language::EN, "Linux");
i18n.registerTranslation(s_strIdBase + STR_OS_MACOS, ui::Language::EN, "macOS");

i18n.registerTranslation(s_strIdBase + STR_OS_WINDOWS, ui::Language::DE, "Windows");
i18n.registerTranslation(s_strIdBase + STR_OS_LINUX, ui::Language::DE, "Linux");
i18n.registerTranslation(s_strIdBase + STR_OS_MACOS, ui::Language::DE, "macOS");
```

**2. Update OS name printing (lines 142-148):**
```cpp
case UnicodeMethod::WINDOWS:
    gfx->print(mstr(STR_OS_WINDOWS));
    break;
case UnicodeMethod::LINUX:
    gfx->print(mstr(STR_OS_LINUX));
    break;
case UnicodeMethod::MACOS:
    gfx->print(mstr(STR_OS_MACOS));
    break;
```

**3. Update footer hint (line 170):**
```cpp
const char* getFooterHint() const override { return mstr(STR_STATUS); }  // Or use core string
```

Actually, for the footer hint, use the core I18n system:
```cpp
const char* getFooterHint() const override { return ui::tr(ui::StringId::HINT_BACK); }
```

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n system definition
- `components/mod_hid/src/HidModule.cpp:68-74` - Existing German translations
- `components/mod_hid/src/HidModule.cpp:211-218` - Correct I18n usage pattern in same file

</content>