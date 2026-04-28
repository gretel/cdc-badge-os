---
title: "[MEDIUM] RgbInputView has hardcoded English \"Preview:\" label"
severity: MEDIUM
domain: component-library-usage
lens: ui-components
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The `RgbInputView` component in the `grove_led` module has a hardcoded English string "Preview:" that should be internationalized using the I18n system.

**Location:**
- `components/grove_led/src/RgbInputView.cpp:249`

## Impact
1. **Language Inconsistency**: The view uses I18n for the footer hint (`HINT_FIELD_NAV` on line 260) but hardcodes "Preview:" in English, creating an inconsistent user experience for German-speaking users.

2. **Module Isolation**: While the grove_led module is self-contained, it should still follow the established I18n pattern used by other views for consistency.

3. **Minor UX Issue**: German users will see mixed English/German UI: "Preview:" in English but German hint text in the footer.

## Evidence
**RgbInputView.cpp (line 249):**
```cpp
gfx->print("Preview:");
```

**Same file, correct I18n usage (line 260):**
```cpp
gfx->print(ui::tr(ui::StringId::HINT_FIELD_NAV));
```

**I18n.h shows available hint strings but no PREVIEW:**
```cpp
// Line 176: Available hint strings
HINT_FIELD_NAV,
HINT_DATE_INPUT,
HINT_TIME_INPUT,
// ... but no PREVIEW label
```

## Recommended Fix
**Option 1: Add I18n string (preferred for consistency)**

1. Add new StringId to `I18n.h` (after `HINT_TIME_INPUT`):
```cpp
PREVIEW_LABEL,
```

2. Add translation in `I18n.cpp` initCoreStrings():
```cpp
REG(PREVIEW_LABEL,    "Preview:",  "Vorschau:");
```

3. Update RgbInputView.cpp line 249:
```cpp
gfx->print(ui::tr(ui::StringId::PREVIEW_LABEL));
```

**Option 2: Quick fix with module-local string**

If adding a core string is considered too broad for a single use case:
```cpp
static const char* getPreviewLabel() {
    return ui::tr( /* module-specific ID if registered */ );
}
// Or simply:
gfx->print("Preview:");  // Keep as is if this is acceptable
```

**Recommended approach: Option 1** since "Preview" is a common UI label that may be reused in other modules.

## References
- `components/cdc_ui/include/cdc_ui/I18n.h` - I18n system definition
- `components/cdc_ui/src/I18n.cpp:363` - Core string initialization
- `components/grove_led/src/RgbInputView.cpp:260` - Correct I18n usage in same file
