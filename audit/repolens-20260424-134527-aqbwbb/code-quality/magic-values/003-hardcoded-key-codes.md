---
title: "[MEDIUM] Hardcoded key codes for UI navigation"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
Character literals are used directly in switch statements and comparisons for UI navigation across multiple view components. These represent keypad button mappings but lack named constants, making the code harder to understand and modify.

**Files affected (15+ occurrences):**
- `components/cdc_views/src/ListView.cpp:140-161` - Navigation keys: `'2'`, `'8'`, `'Y'`, `'3'`, `'N'`
- `components/cdc_views/src/ContextMenuView.cpp:108-120` - Same navigation keys
- `components/cdc_views/src/DateInputView.cpp:174,184` - Confirm keys: `'N'`, `'Y'`
- `components/cdc_views/src/TimeInputView.cpp:133,142` - Confirm keys: `'N'`, `'Y'`
- `components/cdc_views/src/SliderView.cpp:111,117` - Save/Cancel: `'Y'`, `'N'`
- `components/cdc_views/src/PinEntryView.cpp:193,205` - Backspace/Confirm: `'N'`, `'Y'`
- `components/cdc_views/src/T9InputView.cpp:210,239` - Confirm/Back: `'Y'`, `'N'`
- `components/cdc_views/src/ConfirmView.cpp:39,47` - Yes/No: `'Y'`, `'N'`
- `components/cdc_views/src/InfoView.cpp:109,113` - Yes/No callbacks: `'Y'`, `'N'`
- `components/cdc_views/src/MessageBox.cpp:76` - Dismiss: `'Y'`, `'N'`
- `components/cdc_views/src/ToastView.cpp:62` - Dismissible: `'Y'`, `'N'`
- `components/grove_led/src/RgbInputView.cpp:151,161` - Clear/Confirm: `'N'`, `'Y'`

**Key mappings used:**
- `'2'` = Up navigation
- `'8'` = Down navigation  
- `'Y'` = Confirm/Select/Save
- `'N'` = Cancel/Back/Backspace
- `'3'` = Context menu

## Impact
- **Readability**: `case '2':` is less clear than `case KEY_UP:`
- **Maintainability**: Changing keypad layout requires searching through 15+ files
- **Consistency**: All views use the same mapping but have no shared definition
- **Discoverability**: New developers must infer the meaning of each character

## Evidence
```cpp
// components/cdc_views/src/ListView.cpp:140-161
switch (key) {
    case '2': // Up
        navigate(false);
        return InputResult::CONSUMED;

    case '8': // Down
        navigate(true);
        return InputResult::CONSUMED;

    case 'Y': // Select
        if (onSelect_ && items_ && selection_ < itemCount_) {
            onSelect_(selection_, items_[selection_].userData);
        }
        return InputResult::CONSUMED;

    case '3': // Context menu
        if (onMenu_ && items_ && selection_ < itemCount_) {
            onMenu_(selection_, items_[selection_].userData);
            return InputResult::CONSUMED;
        }
        return InputResult::IGNORED;

    case 'N': // Back
        return InputResult::REQUEST_POP;
}
```

Same pattern repeated in ContextMenuView, DateInputView, TimeInputView, SliderView, PinEntryView, T9InputView, ConfirmView, InfoView, MessageBox, ToastView, RgbInputView.

## Recommended Fix
1. **Create key code constants** in `components/cdc_views/include/cdc_views/KeyCodes.h`:
   ```cpp
   #pragma once
   
   // Keypad navigation key codes (TCA9535 12-key layout)
   static constexpr char KEY_UP = '2';
   static constexpr char KEY_DOWN = '8';
   static constexpr char KEY_SELECT = 'Y';      // Yes/OK
   static constexpr char KEY_CANCEL = 'N';      // No/Back
   static constexpr char KEY_CONTEXT = '3';     // Menu
   static constexpr char KEY_BACKSPACE = 'N';   // Same as cancel
   ```

2. **Create InputResult helper methods** to make code more expressive:
   ```cpp
   // In ListView.cpp
   switch (key) {
       case KEY_UP:
           navigate(false);
           return InputResult::CONSUMED;
       case KEY_DOWN:
           navigate(true);
           return InputResult::CONSUMED;
       case KEY_SELECT:
           // ...
   }
   ```

3. **Consider creating a KeyMapping class** for more complex scenarios:
   ```cpp
   class KeyMapping {
       static char actionToKey(UIAction action);
       static UIAction keyToAction(char key);
   };
   ```

4. **Add documentation** explaining the keypad layout:
   ```
   TCA9535 12-key keypad layout:
   1 2 3    (Up, Down, Menu)
   4 5 6
   7 8 9
   * 0 #    (Cancel/N, OK/Y)
   ```

## References
- [TCA9535 datasheet](https://www.ti.com/lit/gpn/TCA9535)
- [Keypad matrix design patterns](https://www.allaboutcircuits.com/projects/how-to-build-a-keypad-matrix/)
