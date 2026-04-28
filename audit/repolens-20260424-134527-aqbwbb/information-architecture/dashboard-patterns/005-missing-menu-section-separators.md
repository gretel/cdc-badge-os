---
title: "[MEDIUM] Missing visual section separators in module-backed menus"
severity: MEDIUM
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

The main menu and tools menu combine dynamically-loaded module items with fixed system items (Tools, Settings) but lack **visual section separators or headers** to help users distinguish between the two groups. This affects:

- `components/cdc_os_ui/src/AppUi.cpp:316-335` - `rebuildMainMenu()`
- `components/cdc_os_ui/src/AppUi.cpp:337-362` - `rebuildToolsMenu()`
- `components/cdc_views/src/ListView.cpp` - ListView rendering

**Current menu structure:**
```
Main Menu:
  [Module item 1]  <- No visual distinction
  [Module item 2]  <- No visual distinction
  [Module item 3]  <- No visual distinction
  Tools           <- Fixed item (same visual style)
  Settings        <- Fixed item (same visual style)
```

Users cannot quickly scan the menu to understand:
- Where module items end and system items begin
- How many modules are installed
- Which items are "dynamic" vs "permanent"

## Impact

**User Experience:**
- Cognitive load increases as users must memorize item positions
- Harder to discover new modules when they're added
- No visual hierarchy to indicate item categories

**Scalability:**
- As module count grows (currently 10 modules), the menu becomes harder to navigate
- No clear way to add "section headers" without breaking the current structure

## Evidence

**Menu Rebuild Logic:**
```cpp
// components/cdc_os_ui/src/AppUi.cpp:316-335
void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    s_mainMenuPluginCount = moduleReg.getMenuItems(
        core::MenuLocation::MAIN_MENU,
        s_mainMenuModuleItems,
        MAIN_MENU_MAX_ITEMS - MAIN_MENU_FIXED_COUNT
    );

    for (uint8_t i = 0; i < s_mainMenuPluginCount; i++) {
        s_mainMenuItems[i] = {s_mainMenuModuleItems[i].label, 0, false, nullptr};
    }

    s_mainMenuItems[getToolsIndex()] = {tr(StringId::TOOLS), 0, false, nullptr};
    s_mainMenuItems[getSettingsIndex()] = {tr(StringId::SETTINGS), 0, false, nullptr};

    if (s_mainMenu) {
        s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, getMainMenuCount());
    }
}
```

**ListView Rendering:**
```cpp
// components/cdc_views/src/ListView.cpp - renders all items with same style
// No special handling for section breaks or category headers
```

**What's Missing:**
- No section separator items (e.g., "--- Modules ---", "--- System ---")
- No visual distinction (indentation, icon, color) for module vs fixed items
- No count indicator showing "X modules installed"

## Recommended Fix

Add visual section separators to improve menu scanability:

**Option 1: Section Header Items (Simple)**

Add separator items between module and fixed sections:
```cpp
void rebuildMainMenu() {
    // ... existing module items ...

    // Add section separator
    s_mainMenuItems[s_mainMenuPluginCount] = {
        "--- System ---",  // Or use a special StringId
        0, true, nullptr   // 'true' = isSectionHeader flag
    };

    s_mainMenuItems[getToolsIndex() + 1] = {tr(StringId::TOOLS), 0, false, nullptr};
    s_mainMenuItems[getSettingsIndex() + 1] = {tr(StringId::SETTINGS), 0, false, nullptr};

    if (s_mainMenu) {
        s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, 
                        getMainMenuCount() + 1);  // +1 for separator
    }
}
```

**Option 2: Enhanced ListItem Structure (More Flexible)**

Extend `ListItem` to support section headers:
```cpp
struct ListItem {
    const char* label;
    uint8_t badge;
    bool isSectionHeader;  // New field
    void* userData;
};
```

Then update `ListView::render()` to draw section headers differently:
```cpp
// Draw section header with different style
if (item->isSectionHeader) {
    gfx->fillRect(0, y, width, itemHeight, EPD_GRAY);
    gfx->setCursor(10, y + 12);
    gfx->print(item->label);
} else {
    // Normal item rendering
}
```

**Implementation Steps:**
1. Add `isSectionHeader` field to `ListItem` struct (~10 min)
2. Update `rebuildMainMenu()` to insert separator (~15 min)
3. Update `ListView::render()` to draw section headers differently (~20 min)
4. Test with varying module counts (~15 min)

Total: ~1 hour

## References

- Dashboard pattern: Section headers improve scanability in long lists
- UX principle: Progressive disclosure - group related items visually
- Similar pattern: iOS Settings app uses section headers for grouping
