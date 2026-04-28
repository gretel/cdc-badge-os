---
title: "[LOW] Inconsistent list-to-detail interaction patterns across modules"
severity: LOW
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

Module list views use different interaction patterns for accessing item details. Found in:

- `components/mod_totp/src/TotpModule.cpp:727-740` - TOTP list (direct view push)
- `components/mod_password/src/PasswordModule.cpp:780-805` - Password list (context menu)
- `components/mod_gpg/src/GpgModule.cpp` - GPG menu (different structure)

**Interaction patterns found:**

| Module | Primary Action | Secondary Actions | Pattern |
|--------|---------------|-------------------|---------|
| TOTP | Select → Show code | Context menu: Edit, Delete | Direct + Context |
| Password | Select → Show detail | Context menu: View, Edit, Delete | Context Menu |
| GPG | Select → Sub-menu | Varies | Menu-based |

Users must learn different interaction patterns for each module instead of having a consistent mental model.

## Impact

**User Experience:**
- Inconsistent navigation requires re-learning per module
- Unclear when to use direct selection vs. context menu
- No predictable pattern for "what does pressing Y do?"

**Design Consistency:**
- TOTP: Pressing Y on an item immediately shows the code (fast for common action)
- Password: Pressing Y opens a context menu (slower, more flexible)
- GPG: Pressing Y opens a sub-menu with options

## Evidence

**TOTP Module Pattern (Direct View Push):**
```cpp
// components/mod_totp/src/TotpModule.cpp:727-740
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();  // First item = add action
        return;
    }
    if (index - 1 >= s_accountCount) return;

    uint16_t slot = s_listSlots[index - 1];
    const char* name = s_listLabels[index - 1];
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);  // Direct push, no menu
}
```

**Password Module Pattern (Context Menu):**
```cpp
// components/mod_password/src/PasswordModule.cpp:780-805
static void onListMenu(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        static ui::ContextMenuItem items[] = {
            {mstr(STR_NEW_ENTRY), []() { wizardStart(); }}
        };
        ui::showContextMenu(mstr(STR_ACTIONS), items, 1);
        return;
    }
    if (index - 1 >= s_entryCount) return;
    s_activeSlot = s_entries[index - 1].slot;

    static ui::ContextMenuItem items[] = {
        {mstr(STR_VIEW), onMenuView},
        {mstr(STR_EDIT), onMenuEdit},
        {mstr(STR_DELETE), onMenuDelete}
    };
    ui::showContextMenu(mstr(STR_ACTIONS), items, 3);
}

static void onListSelect(uint16_t index, void* userData) {
    // Select just shows detail view directly
    if (index == 0) {
        wizardStart();
        return;
    }
    s_activeSlot = s_entries[index - 1].slot;
    ui::ViewStack::instance().push(&s_infoView);  // Direct push
}
```

**What's Different:**
- TOTP: Select → Code view (immediate), Context → Edit/Delete
- Password: Select → Detail view, Context → View/Edit/Delete (redundant!)
- Both use first item for "add" action (consistent)

## Recommended Fix

Establish a consistent interaction pattern for all module list views:

**Recommended Pattern (Fast + Flexible):**

1. **Primary action (Select/Y key):** Show most common detail view
   - TOTP: Show current code (most common action)
   - Password: Show entry details (most common action)
   - GPG: Show key details (most common action)

2. **Context menu (3 key or long-press):** Secondary actions
   - Edit, Delete, Copy, etc.
   - Consistent across all modules

**Implementation Steps:**

1. **Document the pattern** in a MODULE_DEVELOPMENT.md guide:
   ```markdown
   ## List View Pattern
   
   - First item (index 0): Always "Add new" action
   - Data items (index 1+): Press Y → Show detail view
   - Context menu (3 key): Secondary actions (Edit, Delete, etc.)
   ```

2. **Create a shared helper** for common list patterns:
   ```cpp
   // components/cdc_views/ModuleListView.h
   class ModuleListView {
       void setAddAction(const char* label, std::function<void()>);
       void setItemActions(const char* detailLabel, 
                          std::function<void(uint16_t)> detail,
                          std::function<void(uint16_t)> context);
   };
   ```

3. **Update existing modules** to follow the pattern:
   - TOTP: Already follows pattern (good!)
   - Password: Remove redundant "View" from context menu (since Select already shows view)
   - GPG: Document why menu pattern is used (multiple equal-weight actions)

This is a documentation + minor refactoring task (~1 hour).

## References

- Dashboard pattern: Consistent list-to-detail navigation improves usability
- UX principle: Users should have a predictable mental model across similar views
- Reference: Legacy code in `~/GIT/cdc-badge-os-legacy/` may have different patterns to compare
