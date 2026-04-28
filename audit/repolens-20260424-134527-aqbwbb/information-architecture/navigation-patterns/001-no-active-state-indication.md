---
title: "[MEDIUM] No active state indication for current menu item in navigation"
severity: MEDIUM
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The ListView component used for all menus (main menu, tools menu, settings menu, and module menus) does not visually indicate which item is currently active/selected beyond the cursor position. When navigating back from a child view to a parent menu, there is no persistent visual indicator showing which menu item led to the current view.

**Files affected:**
- `components/cdc_views/include/cdc_views/ListView.h` - ListView class definition
- `components/cdc_views/src/ListView.cpp` - ListView implementation
- `components/cdc_os_ui/src/AppUi.cpp` - Menu construction and navigation flow (lines 391-470)

## Impact
Users cannot quickly identify their current location in the navigation hierarchy when returning to a menu. This is particularly problematic for:
1. Deep navigation paths (e.g., Tools → Expert → Modules)
2. Modules with multiple menu entries at different locations
3. Users who need to navigate back to the same section after exploring other features

The lack of active state indication increases cognitive load and requires users to remember their navigation path.

## Evidence
The ListView class tracks selection index (`selection_` at line 138 in ListView.h) but only uses it for cursor positioning, not for persistent active-state styling:

```cpp
// components/cdc_views/include/cdc_views/ListView.h (lines 136-140)
uint16_t selection_ = 0;
uint16_t scrollPos_ = 0;
SelectCallback onSelect_ = nullptr;
```

The navigation callbacks in AppUi.cpp push views without marking which menu item was selected:

```cpp
// components/cdc_os_ui/src/AppUi.cpp (lines 391-405)
static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;
    
    // Module items first
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();
            if (view) ViewStack::instance().push(view);
        }
        return;
    }
    // ...
}
```

No mechanism exists to store or display which menu item is "active" for the current view.

## Recommended Fix
Add active-state indication to ListView by:

1. **Add active index tracking** to ListView (line 138 area):
   ```cpp
   uint16_t activeIndex_ = UINT16_MAX;  // Currently active item
   ```

2. **Update active index on selection** in onKey handler for 'Y' key:
   ```cpp
   if (key == 'Y') {
       activeIndex_ = selection_;  // Mark as active
       if (onSelect_) onSelect_(selection_, items_[selection_].userData);
   }
   ```

3. **Render active state** in render() method - add visual marker (e.g., '*' prefix or different background) to active item.

4. **Reset active index** when view is re-entered or on pop:
   ```cpp
   void onResume() override {
       // Keep activeIndex_ to show which item led here
       dirty_ = true;
   }
   ```

This provides a ~1 hour scope fix that significantly improves navigation clarity.

## References
- Navigation patterns: [Nielsen Norman Group - Navigation](https://www.nngroup.com/articles/navigation-definitions/)
- Active state best practices: Active state should persist until user selects a different item
- Current ListView implementation: `components/cdc_views/src/ListView.cpp`
