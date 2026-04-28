---
title: "[LOW] No keyboard shortcut for quick navigation to common destinations"
severity: LOW
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The navigation system relies entirely on scroll-and-select interaction (2/8 to scroll, Y to select) but provides no keyboard shortcuts for quick navigation to common destinations like main menu, settings, or tools. Users must scroll through lists even for frequently accessed items.

**Files affected:**
- `components/cdc_views/src/ListView.cpp` - Key handling for navigation
- `components/cdc_os_ui/src/AppUi.cpp` - Menu structure

## Impact
Power users must scroll through potentially long lists to reach common items:
- Settings menu has 8 items (need up to 8 scrolls)
- Tools menu has 4+ items
- Main menu can have 16+ items including modules

Common shortcuts in other systems:
- '0' → Main menu
- 'S' → Settings
- 'T' → Tools
- '1-9' → Jump to item N

## Evidence
ListView only handles numeric keys for scrolling and Y/N for actions:

```cpp
// components/cdc_views/src/ListView.cpp - onKey handler
// Keys: 2=Up, 8=Down, Y=Select, N=Back, 3=Menu
if (key == '2') { navigate(false); }  // Up
if (key == '8') { navigate(true); }   // Down
if (key == 'Y') { select(); }         // Select
if (key == 'N') { return REQUEST_POP; }  // Back
if (key == '3') { showContextMenu(); }   // Context
```

No jump-to-item functionality exists. Users must scroll sequentially.

Main menu structure (from AppUi.cpp lines 310-340):
- Module items (dynamic count)
- Tools
- Settings

To reach Settings from top of 10-item menu: 10 scrolls down.

## Recommended Fix
Add keyboard shortcuts for common destinations:

1. **Add jump-to-index in ListView**:
   ```cpp
   InputResult onKey(char key) override {
       // Existing handlers...
       
       // '0' → jump to first item
       if (key == '0' && itemCount_ > 0) {
           setSelection(0);
           return CONSUMED;
       }
       
       // '1'-'9' → jump to item N (if visible)
       if (key >= '1' && key <= '9') {
           uint8_t idx = key - '1';
           if (idx < visibleItems_ && idx < itemCount_) {
               setSelection(scrollPos_ + idx);
               return CONSUMED;
           }
       }
   }
   ```

2. **Add global shortcuts in ViewStack**:
   ```cpp
   void dispatchKey(char key) {
       // Check global shortcuts first
       if (key == '0') {  // Main menu shortcut
           popToRoot();
           return;
       }
       // ...
   }
   ```

3. **Show shortcuts in footer**:
   ```cpp
   // "[0] Home  [1-3] Jump  [Y] Select  [N] Back"
   ```

Scope: ~1 hour to add '0' for home and '1-3' for quick jump.

## References
- ListView key handling: `components/cdc_views/src/ListView.cpp`
- Menu structure: `components/cdc_os_ui/src/AppUi.cpp` lines 310-470
