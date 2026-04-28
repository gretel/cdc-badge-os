---
title: "[LOW] Modules view shows "(none)" without explanation when no modules registered"
severity: LOW
domain: ui-framework
lens: empty-states
labels:
  - "empty-state"
  - "modules"
  - "expert-menu"
---

## Summary
The Modules view in `components/cdc_os_ui/src/ExpertMenuUi.cpp` shows a single "(none)" item when no modules are registered. While this indicates the absence of modules, it doesn't explain why or provide any context for users who might expect modules to be available.

**Location:** `components/cdc_os_ui/src/ExpertMenuUi.cpp:120-155` (rebuildModulesView function)

## Impact
Users seeing "(none)" in the Modules view may:
- Wonder if modules failed to load
- Not understand what modules are or how to enable them
- Be confused about whether this is normal for a fresh install
- Not know where to find module documentation

## Evidence
In `rebuildModulesView()` (lines 120-155):
```cpp
static void rebuildModulesView() {
    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t count = moduleReg.getModuleCount();

    if (count == 0) {
        s_modulesItems[0] = {"(none)", 0, false, nullptr};  // <-- Minimal message
        count = 1;
    } else {
        for (uint8_t i = 0; i < count && i < MODULES_VIEW_MAX; i++) {
            // ... builds module list ...
        }
        if (count > MODULES_VIEW_MAX) count = MODULES_VIEW_MAX;
    }

    if (s_modulesView) {
        s_modulesView->init(tr(StringId::MODULES), s_modulesItems, count);
    }
}
```

The "(none)" string is hardcoded, not translated, and provides no context.

## Recommended Fix
Improve the empty state message to be more descriptive and optionally provide navigation to help:

```cpp
if (count == 0) {
    // More descriptive empty state
    s_modulesItems[0] = {tr(StringId::MODULES_EMPTY), 0, false, nullptr};
    // Optional: Add help item
    s_modulesItems[1] = {tr(StringId::HELP), 0, false, nullptr};
    count = 2;
}
```

Add translation strings:
```cpp
// In cdc_ui/I18n.h
STR_MODULES_EMPTY = "No modules registered",
STR_MODULES_EMPTY_DE = "Keine Module registriert",
```

Or, if the Modules view is meant to always have modules (since modules register themselves at startup), consider showing a more informative message:
```cpp
if (count == 0) {
    s_modulesItems[0] = {tr(StringId::MODULES_LOADING), 0, false, nullptr};
    count = 1;
}
```

## References
- ExpertMenuUi: `components/cdc_os_ui/src/ExpertMenuUi.cpp`
- ModuleRegistry: `components/cdc_core/ModuleRegistry.h`
- I18n strings: `components/cdc_ui/include/cdc_ui/I18n.h`
