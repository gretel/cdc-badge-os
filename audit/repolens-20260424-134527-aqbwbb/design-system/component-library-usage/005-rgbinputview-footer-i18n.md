---
title: "[MEDIUM] RgbInputView footer hint hardcoded in English, bypassing I18n system"
severity: MEDIUM
domain: component-library-usage
lens: ui-components
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The `RgbInputView` component in the `grove_led` module has a hardcoded English footer hint string that bypasses the project's I18n system, while the same component correctly uses I18n for other text.

**Location:**
- `components/grove_led/src/RgbInputView.cpp:178` - Hardcoded footer hint

## Impact
1. **Inconsistent Internationalization**: The same file uses I18n correctly on line 260 (`ui::tr(ui::StringId::HINT_FIELD_NAV)`) but hardcodes the footer hint in English on line 178.

2. **English-Only UI**: Users with German language setting will see English text "0-9:Input 4/6:Field Y:OK" in the footer while other parts of the UI are in German.

3. **Pattern Inconsistency**: All other views in `cdc_views` use `ui::tr(StringId::...)` for their footer hints, but this module-specific view breaks the pattern.

4. **Maintenance Burden**: If the footer format needs to change, developers might not realize this string needs updating since it's not in the central I18n system.

## Evidence
**RgbInputView.cpp (line 178 - hardcoded English):**
```cpp
const char* RgbInputView::getFooterHint() const {
    return "0-9:Input 4/6:Field Y:OK";
}
```

**Same file, correct I18n usage (line 260):**
```cpp
gfx->print(ui::tr(ui::StringId::HINT_FIELD_NAV));
```

**Comparison with ListView (correct pattern from cdc_views):**
```cpp
// components/cdc_views/src/ListView.cpp:173-177
const char* ListView::getFooterHint() const {
    return tr(StringId::HINT_LIST_MENU);
}
```

**Available I18n hint strings (I18n.h:176):**
```cpp
HINT_FIELD_NAV,  // Already exists and is used correctly in same file
HINT_DATE_INPUT,
HINT_TIME_INPUT,
```

## Recommended Fix
**Option 1: Add module-specific I18n string (recommended)**

1. In `components/grove_led/src/GroveLedModule.cpp` (or new file), register module strings:
```cpp
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_RGB_FOOTER = 0;

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("grove_led", 5);  // Reserve 5 IDs
    
    i18n.registerTranslation(s_strIdBase + STR_RGB_FOOTER, ui::Language::EN, "0-9:Input 4/6:Field Y:OK");
    i18n.registerTranslation(s_strIdBase + STR_RGB_FOOTER, ui::Language::DE, "0-9:Eingabe 4/6:Feld Y:OK");
}
```

2. Update `RgbInputView.cpp:178`:
```cpp
const char* RgbInputView::getFooterHint() const {
    return ui::tr(s_strIdBase + STR_RGB_FOOTER);
}
```

**Option 2: Use existing HINT_FIELD_NAV if appropriate**

If the existing `HINT_FIELD_NAV` string is suitable, simply update the footer hint:
```cpp
const char* RgbInputView::getFooterHint() const {
    return ui::tr(ui::StringId::HINT_FIELD_NAV);
}
```

**Recommended: Option 1** - Provides proper module isolation while maintaining I18n consistency.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n system definition
- `components/cdc_views/src/ListView.cpp:173` - Correct I18n usage pattern
- `components/grove_led/src/RgbInputView.cpp:260` - Correct I18n usage in same file

</content>