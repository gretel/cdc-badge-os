---
title: "[HIGH] ConfirmView footer hint hardcoded in German, bypassing I18n system"
severity: HIGH
domain: component-library-usage
lens: ui-components
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The `ConfirmView` component has inconsistent and hardcoded German text for its footer hint, bypassing the project's established I18n (internationalization) system.

**Locations:**
- `components/cdc_views/include/cdc_views/ConfirmView.h:54` - Header declares `getFooterHint()` returning hardcoded German string
- `components/cdc_views/src/ConfirmView.cpp:185` - Implementation also hardcodes German text `"Y=Ja  N=Nein"`

## Impact
1. **I18n Inconsistency**: The `cdc_ui` I18n system is well-established with `StringId::HINT_APPROVE_DENY` and `StringId::HINT_OK_BACK` defined in `I18n.h:167-168` and translations in `I18n.cpp:357-358`, but `ConfirmView` bypasses this entirely.

2. **German-Only UI**: Users with language set to English will see German text `"Y=Ja  N=Nein"` in the ConfirmView dialog, while other views correctly use I18n translations.

3. **Duplication**: Two different German strings exist:
   - Header: `"Y=OK  N=Abbruch"` (line 54 of ConfirmView.h)
   - Source: `"Y=Ja  N=Nein"` (line 185 of ConfirmView.cpp)
   
   This creates confusion and maintenance burden.

4. **Pattern Violation**: Other views like `ListView`, `PinEntryView`, `SliderView` all use `ui::tr(StringId::...)` for their footer hints, but `ConfirmView` is an outlier.

## Evidence
**ConfirmView.h (line 54):**
```cpp
const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }
```

**ConfirmView.cpp (line 185):**
```cpp
gfx->print("Y=Ja  N=Nein");
```

**I18n system (already available):**
```cpp
// I18n.h:167-168
REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");

// Usage pattern from other views:
// ListView.cpp:173
const char* ListView::getFooterHint() const {
    return tr(StringId::HINT_LIST_MENU);
}
```

## Recommended Fix
1. **Update ConfirmView.h** to use I18n string:
```cpp
const char* getFooterHint() const override { return tr(StringId::HINT_APPROVE_DENY); }
```

2. **Update ConfirmView.cpp** to use I18n string (line 185):
```cpp
gfx->print(tr(StringId::HINT_APPROVE_DENY));
```

3. **Add include** if needed:
```cpp
#include "cdc_ui/I18n.h"  // For tr() function
```

4. **Verify** the component still compiles and the footer hint displays correctly in both English and German.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n system definition
- `components/cdc_ui/src/I18n.cpp` - Core string translations
- `components/cdc_views/src/ListView.cpp:173` - Example of correct I18n usage in views
- `components/cdc_views/src/PinEntryView.cpp:218` - Another example of correct I18n usage
