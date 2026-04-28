---
title: "[LOW] Inconsistent drill-down patterns across module views"
severity: LOW
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

Module views use inconsistent patterns for drill-down navigation from list items to detail views. This affects:

- `components/mod_totp/src/TotpModule.cpp:727-739` - TOTP list to detail view
- `components/mod_password/src/PasswordModule.cpp` - Password list navigation
- `components/mod_gpg/src/GpgModule.cpp` - GPG menu navigation
- `components/mod_sao/src/SaoModule.cpp` - SAO info view

**Inconsistencies found:**
1. **Index offset handling varies** - TOTP uses `index - 1` to skip header item, others may differ
2. **Detail view presentation differs** - Some show codes inline, others push new views
3. **No consistent "context" passing** - Filter context (e.g., which account selected) not standardized

## Impact

**User Experience:**
- Navigation behavior differs between modules, requiring re-learning
- Users cannot predict if selection shows detail inline or pushes new screen
- Inconsistent back-button behavior across modules

**Maintenance:**
- New module developers have no pattern to follow
- Harder to implement global navigation features (e.g., "back to overview")

## Evidence

**TOTP Module Pattern:**
```cpp
// components/mod_totp/src/TotpModule.cpp:727-739
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();  // First item = add action
        return;
    }
    if (index - 1 >= s_accountCount) return;

    uint16_t slot = s_listSlots[index - 1];  // Offset by 1 for header
    const char* name = s_listLabels[index - 1];
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);  // Pushes detail view
}
```

**SAO Module Pattern (different):**
```cpp
// components/mod_sao/src/SaoModule.cpp
items[0] = {
    mstr(STR_SAO),
    120,
    getInfoView,  // Returns pre-built info view directly
    nullptr,
    getName(),
    core::MenuLocation::TOOLS_MENU,
    nullptr
};
```

**GPG Module Pattern (yet another):**
```cpp
// components/mod_gpg/src/GpgModule.cpp
items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
    // ... validation ...
    rebuildMenu();
    return &s_menuView;  // Returns menu, not detail view
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

**What's Missing:**
- No standard convention for "first item = action" vs. "all items = data"
- No standard pattern for list-to-detail navigation
- No shared helper for common drill-down flows

## Recommended Fix

Establish and document a consistent module view pattern:

1. **Document the convention** in `docs/MODULE_DEVELOPMENT.md`:
   - First list item can be action (e.g., "Add") with index 0
   - Data items start at index 1, offset calculation: `dataIndex = index - 1`
   - Detail views should be pushed via `ViewStack::instance().push()`

2. **Create a shared helper** in `components/cdc_views/`:
   ```cpp
   // Helper for common list-to-detail pattern
   void showDetailView(IView* detailView, const char* title, 
                       uint16_t dataIndex, void* userData);
   ```

3. **Update existing modules** to follow the pattern:
   - TOTP: Already follows pattern (good!)
   - SAO: Consider making interactive (currently static info)
   - GPG: Document why menu pattern is used (multiple actions)

This is a documentation + refactoring task that can be split into:
- 30 min: Document the pattern
- 30 min per module: Refactor to follow pattern (if needed)

## References

- Dashboard pattern: List-to-detail navigation should be consistent
- Reference: Legacy code in `~/GIT/cdc-badge-os-legacy/` may have different patterns to compare
