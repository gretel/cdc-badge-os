---
title: "[MEDIUM] No active-state indication in menu navigation"
severity: MEDIUM
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The ListView component does not track or display which menu item corresponds to the currently active view. When users navigate to a module view (e.g., TOTP, GPG) and return to the main menu, there is no visual indication of which menu item they just visited or which module is currently "active".

**Evidence locations:**
- `components/cdc_views/include/cdc_views/ListView.h` - No active-state tracking
- `components/cdc_os_ui/src/AppUi.cpp:393` - Menu selection doesn't set active state
- `components/mod_totp/src/TotpModule.cpp:550` - Menu item returns view but no active-state tracking

## Impact
**User orientation:** Users cannot quickly see which feature they last used or which module is currently active.
**Navigation efficiency:** Users must remember their place in the menu structure, especially with 10+ modules.
**Accessibility:** No visual cue for current location in the navigation hierarchy.

## Evidence
```cpp
// ListView has no active item tracking
struct ListItem {
    const char* label;
    uint8_t icon;
    bool iconDisabled;
    void* userData;
    // No "active" field!
};

// Menu selection just pushes view, no state tracking
static void onMainMenuSelect(uint16_t index, void* userData) {
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();
            if (view) ViewStack::instance().push(view);  // No active state set
        }
        return;
    }
}
```

## Recommended Fix
Add active-state support to ListView:
1. **Add `activeIndex` field** to ListView class
2. **Update active index** when view is selected
3. **Visual indicator** (e.g., asterisk, different color, arrow) for active item
4. **Clear active state** when returning to menu (optional, configurable)

Example:
```cpp
struct ListItem {
    const char* label;
    uint8_t icon;
    bool iconDisabled;
    void* userData;
    bool active;  // New field
};

// In onMainMenuSelect
s_mainMenuItems[getToolsIndex()].active = false;  // Clear all
s_mainMenuModuleItems[index].active = true;       // Set selected
```

## References
- `components/cdc_views/include/cdc_views/ListView.h` - List item structure
- `components/cdc_os_ui/src/AppUi.cpp` - Menu selection handlers
