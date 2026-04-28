---
title: "[MEDIUM] Inconsistent UI component library namespace usage across modules"
severity: MEDIUM
domain: component-library-usage
lens: design-system
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The `cdc_views` and `cdc_ui` component library types (`ListItem`, `ContextMenuItem`) are referenced with **inconsistent namespace qualification** across different modules in the codebase.

**Files affected:**
- `components/mod_nvsedit/src/NvsEditModule.cpp` (line 21, 52-53, 359-363)
- `components/cdc_os_ui/src/WifiMenuUi.cpp` (line 20, 68-73, 157)

**Pattern used in affected files:**
```cpp
using namespace cdc::ui;  // Line 21 in NvsEditModule.cpp
static ListItem s_namespaceItems[MAX_NAMESPACES];  // Line 52
static ContextMenuItem s_nsContextItems[] = {  // Line 359
```

**Pattern used in most other modules:**
```cpp
// mod_password/src/PasswordModule.cpp (line 344, 714)
static ui::ListItem* s_listItems = nullptr;
static ui::ContextMenuItem items[] = {

// mod_fido2/src/Fido2Ui.cpp (line 90, 268)
static ui::ListItem s_listItems[FIDO2_MAX_CREDENTIALS];
static ui::ContextMenuItem items[3];

// mod_totp/src/TotpModule.cpp (line 553, 804)
static ui::ListItem* s_listItems = nullptr;
static ui::ListItem digitsItems[3] = {

// grove_led/src/GroveLedModule.cpp (line 173, 181)
static ui::ListItem s_mainMenuItems[5];
```

## Impact
- **Maintainability**: Inconsistent code style makes the codebase harder to read and understand
- **Refactoring risk**: If `cdc::ui` namespace changes, files using `using namespace` may have more subtle conflicts
- **Team consistency**: New developers may not know which style to follow
- **Code reviews**: Additional cognitive load to recognize both patterns as equivalent

## Evidence
**mod_nvsedit/src/NvsEditModule.cpp:**
```cpp
// Line 21
using namespace cdc::ui;
using namespace cdc::core;

// Line 52-53 (uses bare ListItem)
static ListItem s_namespaceItems[MAX_NAMESPACES];
static ListItem s_keyItems[MAX_KEYS];

// Line 359-363 (uses bare ContextMenuItem)
static ContextMenuItem s_nsContextItems[] = {
    {"Delete NS", onDeleteNamespace}
};
static ContextMenuItem s_keyContextItems[] = {
    {"Delete Key", onDeleteKey}
};
```

**mod_password/src/PasswordModule.cpp (consistent pattern):**
```cpp
// Line 344, 714 (uses explicit ui:: prefix)
static ui::ListItem* s_listItems = nullptr;
static ui::ContextMenuItem items[] = {
    {mstr(STR_NEW_ENTRY), []() { wizardStart(); }}
};
```

## Recommended Fix
**Option 1 (Recommended):** Remove `using namespace cdc::ui;` from affected files and use explicit `ui::` prefix consistently:

1. In `components/mod_nvsedit/src/NvsEditModule.cpp`:
   - Remove line 21: `using namespace cdc::ui;`
   - Change line 52-53: `static ui::ListItem s_namespaceItems[MAX_NAMESPACES];`
   - Change line 359-363: `static ui::ContextMenuItem s_nsContextItems[] = {`

2. In `components/cdc_os_ui/src/WifiMenuUi.cpp`:
   - Remove line 20: `using namespace cdc::ui;`
   - Change all `ListItem` references to `ui::ListItem`

**Option 2:** Document the `using namespace cdc::ui;` pattern as acceptable and apply it consistently to all modules (more work, less precise).

**Estimated effort:** ~30 minutes for Option 1.

## References
- `components/cdc_views/include/cdc_views/ListView.h` - Definition of `ListItem` struct
- `components/cdc_views/include/cdc_views/ContextMenuView.h` - Definition of `ContextMenuItem` struct
- `components/cdc_ui/include/cdc_ui/IView.h` - `cdc::ui` namespace definition
