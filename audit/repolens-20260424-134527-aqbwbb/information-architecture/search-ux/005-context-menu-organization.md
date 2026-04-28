---
title: "[MEDIUM] Context menu lacks consistent action organization and discoverability"
severity: MEDIUM
domain: search-ux
lens: information-architecture
labels:
  - "audit:information-architecture/search-ux"
---

## Summary

The `ContextMenuView` component and its usage across modules have inconsistent action organization that affects how users discover and access list-related actions (which would include future search/filter functionality). Issues found:

1. **FIDO2 Module** (`components/mod_fido2/src/Fido2Ui.cpp:258-285`) - Context menu shows Details, Delete, Cancel (Cancel is redundant)
2. **Password Module** (`components/mod_password/src/PasswordModule.cpp:708-728`) - Context menu shows View, Edit, Delete (no clear grouping)

The context menu is the likely entry point for future search/filter actions, but current implementation has:
- No visual grouping of related actions
- Inconsistent ordering (alphabetical vs. frequency-based)
- No indication of keyboard shortcuts within menu items
- Missing "Search" action placeholder for future implementation

## Impact

**Discoverability Impact:**
- Users may not find less common actions (like future search/filter)
- No logical grouping makes it harder to scan the menu
- Inconsistent patterns across modules increase cognitive load

**Action Organization:**
- "Cancel" item in FIDO2 menu is redundant (N key already closes menu)
- No grouping of "View/Edit" together vs. "Delete" (destructive action)
- Future search actions have no clear placement

**Navigation Efficiency:**
- Users must scan all items instead of recognizing groups
- No keyboard hints (e.g., "Search (3)") shown in menu

## Evidence

**FIDO2 Context Menu** (`components/mod_fido2/src/Fido2Ui.cpp:258-285`):
```cpp
static ui::ContextMenuItem items[3];
items[0] = {mstr(STR_DETAILS), []() { showDetail(...); }};
items[1] = {ui::tr(ui::StringId::DELETE), []() { handleDelete(...); }};
items[2] = {ui::tr(ui::StringId::CANCEL), []() {}};  // Redundant - N key already closes
showContextMenu(title, items, 3);
```

**Password Context Menu** (`components/mod_password/src/PasswordModule.cpp:708-728`):
```cpp
static ui::ContextMenuItem items[] = {
    {mstr(STR_VIEW), onMenuView},
    {mstr(STR_EDIT), onMenuEdit},
    {mstr(STR_DELETE), onMenuDelete}
};
showContextMenu(mstr(STR_ACTIONS), items, 3);
```

**ContextMenuView Implementation** (`components/cdc_views/src/ContextMenuView.cpp`):
- Renders items in simple vertical list
- No support for separators or grouping
- No keyboard hint display per item

## Recommended Fix

**Short-term (45 min):** Improve action organization and prepare for search:

1. **Remove redundant Cancel** from FIDO2 menu:
   ```cpp
   // Before: 3 items including Cancel
   static ui::ContextMenuItem items[3] = {
       {mstr(STR_DETAILS), ...},
       {ui::tr(ui::StringId::DELETE), ...},
       {ui::tr(ui::StringId::CANCEL), {}}  // Redundant
   };
   
   // After: 2 items, N key closes automatically
   static ui::ContextMenuItem items[2] = {
       {mstr(STR_DETAILS), ...},
       {ui::tr(ui::StringId::DELETE), ...}
   };
   ```

2. **Group destructive actions** with visual separator (when supported):
   ```cpp
   // Password module - group View/Edit together, separate Delete
   static ui::ContextMenuItem items[] = {
       {mstr(STR_VIEW), onMenuView},
       {mstr(STR_EDIT), onMenuEdit},
       // Separator (future enhancement)
       {mstr(STR_DELETE), onMenuDelete}
   };
   ```

3. **Add Search placeholder** for future implementation:
   ```cpp
   // In ListView context menu when items exist
   static ui::ContextMenuItem items[] = {
       {mstr(STR_VIEW), onMenuView},
       {mstr(STR_EDIT), onMenuEdit},
       {mstr(STR_SEARCH), onMenuSearch},  // Future: opens T9 search
       {mstr(STR_DELETE), onMenuDelete}
   };
   ```

**Medium-term (1 hour):** Enhance ContextMenuView:

4. **Add separator support** to ContextMenuView:
   ```cpp
   struct ContextMenuItem {
       const char* label;
       void (*callback)();
       bool isSeparator;  // New: draw line instead of item
   };
   ```

5. **Add keyboard hint** per item:
   ```cpp
   struct ContextMenuItem {
       const char* label;
       void (*callback)();
       char keyHint;  // e.g., '1', '2', '3' for quick select
   };
   ```

**Files to modify:**
- `components/mod_fido2/src/Fido2Ui.cpp` - Remove redundant Cancel item
- `components/mod_password/src/PasswordModule.cpp` - Consider action grouping
- `components/cdc_views/include/cdc_views/ContextMenuView.h` - Add separator/keyHint fields
- `components/cdc_views/src/ContextMenuView.cpp` - Render separators and hints

## References

- Menu organization patterns: https://www.nngroup.com/articles/menu-design/
- Action sheets vs. context menus: https://material.io/design/components/sheets-bottom.html
- Destructive action placement: https://developer.apple.com/design/human-interface-guidelines/buttons#Action-Buttons

</content>