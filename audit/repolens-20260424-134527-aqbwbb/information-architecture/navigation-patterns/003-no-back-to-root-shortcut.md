---
title: "[MEDIUM] Navigation hierarchy lacks "Back to parent" shortcut"
severity: MEDIUM
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The navigation system only provides sequential back navigation (key 'N' pops one level) but lacks a shortcut to return directly to the main menu or root level. Users navigating deep hierarchies must press 'N' multiple times to return to the main menu.

**Files affected:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - View stack operations (lines 48-55)
- `components/cdc_views/src/ListView.cpp` - Key handling for 'N' (back key)

## Impact
For users navigating 4-5 levels deep (e.g., Main Menu → Tools → Expert → Module → Detail), returning to the main menu requires 4-5 consecutive 'N' key presses. This is inefficient and frustrating, especially for power users who frequently navigate between sections.

Compare to common patterns:
- Long-press 'N' → return to root
- Key '0' → home/main menu
- Menu item "Home" in context menu

## Evidence
ViewStack provides `popToRoot()` but no UI shortcut uses it:

```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h (lines 48-50)
/**
 * Pop all views except root
 */
void popToRoot();
```

ListView's onKey handler only implements single-pop for 'N':

```cpp
// components/cdc_views/src/ListView.cpp (search for onKey)
if (key == 'N') {
    return InputResult::REQUEST_POP;  // Only pops one level
}
```

No long-press handler exists for 'N' to trigger popToRoot():

```cpp
// ViewStack dispatches long press but no view handles it
void dispatchLongPress(char key);  // Line 82 in ViewStack.h
```

The lock screen shows as root, but there's no quick way to return there from deep navigation.

## Recommended Fix
Add long-press 'N' to return to main menu:

1. **Add long-press callback to ListView** (lines 70-80 area):
   ```cpp
   using LongPressCallback = void(*)(uint16_t currentDepth);
   void setOnLongPress(LongPressCallback callback) { onLongPress_ = callback; }
   ```

2. **Handle long-press 'N' in ListView.onKey** (or add to onLongPress):
   ```cpp
   InputResult onLongPress(char key) override {
       if (key == 'N') {
           return InputResult::REQUEST_POP;  // Will trigger popToRoot
       }
       return InputResult::IGNORED;
   }
   ```

3. **Update ViewStack to handle long-press**:
   ```cpp
   void dispatchLongPress(char key) {
       auto* view = current();
       if (key == 'N' && view) {
           auto result = view->onLongPress(key);
           if (result == InputResult::REQUEST_POP) {
               popToRoot();  // Skip to root
           }
       }
   }
   ```

4. **Update footer hint** to show long-press shortcut:
   ```cpp
   // In ListView footer
   "[N] Back  [N long] Home"
   ```

Scope: ~1 hour to implement long-press 'N' to main menu.

## References
- ViewStack popToRoot: `components/cdc_ui/include/cdc_ui/ViewStack.h` line 48
- ListView key handling: `components/cdc_views/src/ListView.cpp`
- Long-press support: `components/cdc_ui/include/cdc_ui/IView.h` line 78
