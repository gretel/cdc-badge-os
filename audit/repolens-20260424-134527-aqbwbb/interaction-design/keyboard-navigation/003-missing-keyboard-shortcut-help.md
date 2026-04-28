---
title: "[LOW] No discoverable keyboard shortcut reference for keypad navigation"
severity: LOW
domain: interaction-design
lens: keyboard-navigation
labels:
  - keyboard-shortcuts
  - discoverability
  - documentation
---

## Summary

The application has no built-in keyboard shortcut reference or help overlay. Users must memorize the keypad navigation scheme (2=Up, 8=Down, Y=Select, N=Back, 4=Left, 6=Right) or find it in external documentation.

**Evidence locations:**
- `components/cdc_views/include/cdc_views/ListView.h` - Keys documented in header (lines 26-31)
- `components/cdc_views/include/cdc_views/ContextMenuView.h` - Keys documented in header (lines 24-27)
- `components/cdc_views/include/cdc_views/PinEntryView.h` - Keys documented in header (lines 17-19)
- `components/cdc_os_ui/src/AppUi.cpp` - Main UI entry point with no help function

Key navigation is documented in individual view headers, but there is no centralized, discoverable help overlay accessible from anywhere in the UI.

## Impact

**User Experience Impact:**
- New users have no way to discover available keyboard shortcuts
- Users may forget navigation keys between uses
- No quick reference when users get "stuck" in a menu
- Increased learning curve for keyboard-only users

**Accessibility Impact:**
- Keyboard users (including power users and those with mobility impairments) benefit from discoverable shortcuts
- No help overlay accessible via a standard key (typically `?` or a dedicated help key)

## Evidence

**File: `components/cdc_views/include/cdc_views/ListView.h` (lines 26-31)**
```cpp
/**
 * ListView - Highly scrollable selection menu
 *
 * Reusable component for any list-based UI.
 * Dynamically calculates visible items based on display size.
 *
 * Keys:
 *   2 = Up
 *   8 = Down
 *   Y = Select (triggers callback)
 *   N = Back (REQUEST_POP)
 */
```

Documentation exists but is buried in header files, not accessible at runtime.

**File: `components/cdc_os_ui/src/AppUi.cpp` - No help function**

Searching for any help-related functionality:
```cpp
// No find result for "help", "shortcut", "keybind" in AppUi.cpp
```

**Footer hints in views** (e.g., `ListView::getFooterHint()`):
```cpp
const char* ListView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    return tr(StringId::HINT_OK_BACK);  // Generic "Y=OK N=Back"
}
```

Only shows basic confirm/back hints, not full navigation help.

## Recommended Fix

1. **Create a KeyboardHelpView** component:
   ```cpp
   /**
    * KeyboardHelpView - Displays all available keyboard shortcuts
    */
   class KeyboardHelpView : public ViewBase {
   public:
       void init();  // Populate with all shortcuts
       void render(bool partial) override;
       InputResult onKey(char key) override;  // Any key to close
   };

   void showKeyboardHelp();  // Modal overlay
   ```

2. **Add universal help key binding** (e.g., `H` or `?`):
   ```cpp
   void ViewStack::dispatchKey(char key) {
       // Check for global help key
       if (key == 'H' || key == '?') {
           showKeyboardHelp();
           return;
       }
       // ...
   }
   ```

3. **Add help footer to menus** when space allows:
   ```cpp
   const char* ListView::getFooterHint() const {
       if (customHint_) {
           return customHint_;
       }
       return tr(StringId::HINT_OK_BACK_H);  // "Y=OK N=Back H=Help"
   }
   ```

4. **Document all keyboard shortcuts** in a centralized location:
   - `components/cdc_ui/include/cdc_ui/KeyboardShortcuts.h`
   - Include in Doxygen documentation
   - Display in the help overlay

5. **Keyboard shortcut reference table** for the help overlay:
   | Key | Action |
   |-----|--------|
   | 2 | Navigate Up |
   | 8 | Navigate Down |
   | 4 | Previous field / Decrease |
   | 6 | Next field / Increase |
   | Y | Select / Confirm |
   | N | Back / Cancel |
   | 3 | Context menu |
   | H | Show help |

## References

- WAI-ARIA Authoring Practices for Keyboard Navigation: https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/
- Common keyboard shortcut patterns in embedded UIs
- ESP32-S3 CDC Badge 12-button keypad layout
