---
title: "[MEDIUM] RgbInputView tightly coupled to grove_led module"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `RgbInputView` class is a generic RGB color input component that belongs in the `cdc_views` module (shared views) but is currently embedded in the `grove_led` module. This creates an inverted dependency where a general-purpose UI component is trapped inside a specific feature module, making it difficult to reuse elsewhere.

**Evidence:**
- File location: `components/grove_led/include/grove_led/RgbInputView.h`
- File location: `components/grove_led/src/RgbInputView.cpp`
- Currently only used by `GroveLedModule.cpp` within the same module
- The view itself has no dependency on LED-specific logic - it's a generic color picker

## Impact
1. **Reusability blocked**: If another module needs RGB input (e.g., password module for custom colors, theme settings), they cannot easily reuse this view
2. **Module bloat**: `grove_led` contains code that isn't specific to LED control
3. **Inconsistent architecture**: Other generic views (ListView, SliderView, PinEntryView) are in `cdc_views`, but `RgbInputView` is an outlier
4. **Discoverability issue**: Developers looking for reusable views won't find `RgbInputView` in `cdc_views`

## Evidence
**RgbInputView.h** shows generic RGB input with no LED-specific code:
```cpp
namespace cdc::grove_led {

class RgbInputView : public ui::ViewBase {
    // RGB color input with R/G/B fields (0-255 each)
    // Navigation: 0-9 = Enter digits, 4 = Previous, 6 = Next, N = Clear, Y = Confirm
    
    void setOnConfirm(ConfirmCallback callback);
    uint8_t getR() const { return r_; }
    uint8_t getG() const { return g_; }
    uint8_t getB() const { return b_; }
    
    // Purely generic: title, r_, g_, b_, currentField_, digitPos_
};
} // namespace cdc::grove_led
```

**CMakeLists.txt** shows grove_led depends on cdc_views:
```cmake
idf_component_register(
    SRCS "src/GroveLedModule.cpp" "src/RgbInputView.cpp"
    INCLUDE_DIRS "include"
    REQUIRES cdc_core cdc_ui cdc_views cdc_hal cdc_log led_strip nvs_flash freertos CalEPD
)
```

## Recommended Fix
1. Move `RgbInputView.h` to `components/cdc_views/include/cdc_views/` (5 minutes)
2. Move `RgbInputView.cpp` to `components/cdc_views/src/` (5 minutes)
3. Update `components/cdc_views/CMakeLists.txt` to add `RgbInputView.cpp` to SRCS (2 minutes)
4. Update include in `grove_led/src/GroveLedModule.cpp`: Change `#include "grove_led/RgbInputView.h"` to `#include "cdc_views/RgbInputView.h"` (2 minutes)
5. Change namespace from `cdc::grove_led` to `cdc::ui` in both files (5 minutes)
6. Update `GroveLedModule.h` include path (2 minutes)
7. Test compilation (5 minutes)

**Total estimated time: ~30 minutes**

## Related Issues
This is similar to the `RenderHelpers` pattern in `cdc_views` - generic rendering utilities that serve multiple views.

## References
- Existing pattern: `components/cdc_views/` contains ListView, SliderView, PinEntryView, DateInputView, TimeInputView, MessageBox, T9InputView, InfoView, ToastView, ContextMenuView, QRCodeView, ConfirmView, RenderHelpers
- Module architecture documentation in `CLAUDE.md`
