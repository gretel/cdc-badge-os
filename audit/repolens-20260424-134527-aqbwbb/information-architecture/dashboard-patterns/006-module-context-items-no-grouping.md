---
title: "[LOW] Lock screen context menu lacks module grouping"
severity: LOW
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

The lock screen context menu (opened with key '3') displays module action items mixed with the built-in "Light" toggle without any visual grouping or module labels. Found in:

- `components/cdc_os_ui/src/views/LockScreenView.cpp:308-335` - Context menu construction
- `components/cdc_core/src/ModuleRegistry.cpp:319-348` - Module context item collection

**Current menu structure:**
```
ACTIONS
  Light        <- Built-in item
  LED On/Off   <- From grove_led module (no indication)
  Badge Text   <- From vcard module (no indication)
```

Users cannot tell:
- Which module provides which action
- How many modules are installed
- Whether actions are grouped by category

## Impact

**User Experience:**
- Confusion about where actions come from
- Harder to discover what modules are installed
- No visual hierarchy in the context menu

**Maintainability:**
- Adding new context menu items requires understanding the callback array limitation
- Hard-coded callback array (`s_moduleCallbacks[]`) limits to 7 module items

## Evidence

**Context Menu Construction:**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:318-331
InputResult LockScreenView::onKey(char key) {
    if (key == '3') {
        uint8_t itemCount = 0;

        // First item: Light toggle (built-in)
        s_contextItems[itemCount++] = {tr(StringId::LIGHT), onLightMenuCallback};

        // Get module items from registry
        auto& moduleReg = core::ModuleRegistry::instance();
        s_moduleContextCount = moduleReg.getLockScreenContextItems(s_moduleContextItems, MAX_CONTEXT_ITEMS - 1);

        // Add module items to context menu
        for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
            const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
            s_contextItems[itemCount++] = {label, s_moduleCallbacks[i]};  // No module info shown
        }

        showContextMenu(tr(StringId::ACTIONS), s_contextItems, itemCount);
        return InputResult::CONSUMED;
    }
    // ...
}
```

**Hard-coded Callback Limitation:**
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp:320-330
static constexpr uint8_t MAX_CONTEXT_ITEMS = 8;
static ContextMenuItem s_contextItems[MAX_CONTEXT_ITEMS];
static core::LockScreenContextItem s_moduleContextItems[MAX_CONTEXT_ITEMS - 1];

static void (*const s_moduleCallbacks[])() = {
    moduleContextCallback0, moduleContextCallback1, moduleContextCallback2,
    moduleContextCallback3, moduleContextCallback4, moduleContextCallback5,
    moduleContextCallback6  // Only 7 module callbacks!
};
```

**What's Missing:**
- No module name shown in context menu items
- No visual grouping (e.g., separator lines, indentation)
- No indication of how many modules provide actions

## Recommended Fix

Add module identification to context menu items:

**Option 1: Simple Module Name Prefix (Easiest)**

Update context menu construction to show module names:
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp
for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
    const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
    const char* moduleName = s_moduleContextItems[i].moduleName ?: "Module";
    
    // Format: "LED (grove_led)" or "Badge Text (vcard)"
    static char formattedLabel[32];
    snprintf(formattedLabel, sizeof(formattedLabel), "%s (%s)", label, moduleName);
    
    s_contextItems[itemCount++] = {formattedLabel, s_moduleCallbacks[i]};
}
```

**Option 2: Section Separator (Better Visual Grouping)**

Add a separator between built-in and module items:
```cpp
// After built-in item
s_contextItems[itemCount++] = {"--- Modules ---", nullptr};  // Special marker

// Then module items
for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
    // ... same as above
}
```

**Option 3: Enhanced ContextMenuItem (Most Flexible)**

Extend `ContextMenuItem` to support module info:
```cpp
struct ContextMenuItem {
    const char* label;
    void (*callback)();
    const char* moduleName;  // New field
    bool isSectionHeader;    // New field
};
```

Then update `ContextMenuView::render()` to draw module items with indentation or different style.

**Implementation Recommendation:**

Start with **Option 1** (simple prefix) as it's ~30 min of work:
1. Add `moduleName` to `LockScreenContextItem` struct if not already there
2. Update context menu construction to format labels with module names
3. Test with 2-3 modules having context items

Then consider **Option 2** for better visual grouping if needed.

## References

- Dashboard pattern: Context menus should show source/origin of actions
- UX principle: Users should understand where actions come from
- Similar pattern: Android context menus show app names for actions
