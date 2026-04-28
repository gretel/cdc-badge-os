---
title: "[LOW] No active/press feedback for list item selection"
severity: LOW
domain: interaction-design
lens: interactive-feedback
labels:
  - "interactive-feedback"
---

## Summary
When a user presses the Y key to select an item in ListView, there's no visual "active" or "pressed" state to confirm the selection was registered before the callback executes and the view changes.

**Location:** `components/cdc_views/src/ListView.cpp:140-150`

The selection callback is called immediately without any visual feedback that the action was triggered.

## Impact
- **Uncertain feedback**: On E-Paper displays with potential refresh latency, users may not know if their press was registered
- **Double-press tendency**: Users might press again wondering if the first press worked
- **Poor perceived responsiveness**: Lack of immediate visual confirmation feels less responsive

## Evidence
```cpp
// components/cdc_views/src/ListView.cpp:140-150
InputResult ListView::onKey(char key) {
    switch (key) {
        case 'Y': // Select
            if (onSelect_ && items_ && selection_ < itemCount_) {
                onSelect_(selection_, items_[selection_].userData);  // Callback fires immediately
            }
            return InputResult::CONSUMED;  // No visual feedback before callback

        case 'N': // Back
            return InputResult::REQUEST_POP;
        // ...
    }
}

// components/cdc_views/src/ListView.cpp:208-230
// Render shows selected state (inverted colors) but no "pressed" state
if (isSelected) {
    gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
} else {
    gfx->setTextColor(EPD_BLACK);
}
```

**Current behavior:**
- Selected item has inverted colors (black background, white text)
- When Y is pressed, the callback fires immediately
- If the callback pushes a new view, the user never sees any "pressed" feedback
- The transition is instant with no intermediate visual state

## Recommended Fix
Add a brief visual "pressed" state before the callback executes:

1. **Show a pressed indicator on the selected row**:
   ```cpp
   // Add a 'pressed_' state variable to ListView
   void ListView::onKey(char key) {
       switch (key) {
           case 'Y':
               if (onSelect_) {
                   // Show pressed state first
                   pressed_ = true;
                   dirty_ = true;
                   // Schedule callback after brief delay or on next render
                   // Or call callback immediately but render pressed state first
               }
       }
   }
   ```

2. **Alternative: Add a small delay with visual feedback**:
   ```cpp
   // Store callback and execute after showing pressed state
   // Render shows pressed state (e.g., darker highlight or border)
   // After 100-200ms, execute callback and push new view
   ```

3. **Simpler approach: Visual press indicator in render()**:
   ```cpp
   // When pressed_ is true, draw additional indicator
   if (isSelected && pressed_) {
       // Draw pressed indicator (e.g., double border, different fill)
       gfx->fillRect(1, y, rowWidth, itemHeight_, EPD_BLACK);  // Deeper highlight
   }
   ```

**Scope:** ~1 hour implementation
- Add `pressed_` state variable to ListView
- Modify `onKey()` to set pressed state
- Update `render()` to show pressed visual state
- Consider E-Paper refresh characteristics for timing

## References
- Material Design: Pressed state patterns
- E-Paper display refresh optimization (partial vs full)
- Existing `dirty_` flag pattern for render scheduling
