---
title: "[MEDIUM] grove_led: View class exposed in module's public include directory"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `grove_led` module exposes a view class (`RgbInputView.h`) in its public include directory (`components/grove_led/include/grove_led/`), making it part of the module's public API when it should be an internal implementation detail:

- **`RgbInputView.h`** - A specialized view for RGB color input, used only by the Grove LED module's menu system
- Located at `components/grove_led/include/grove_led/RgbInputView.h` (public path)
- Used internally by `GroveLedModule.cpp` for the color selection UI

## Impact

- **Unnecessary Public API**: External modules can depend on a view that is really just a UI helper
- **Tight Coupling**: Other modules might import `RgbInputView` and couple themselves to grove_led's implementation
- **Refactoring Risk**: Changing the view's interface would break external consumers
- **Inconsistent with Architecture**: Views should be in `cdc_views/` for reuse, not in module-specific includes

## Evidence

**Current include structure:**
```
components/grove_led/include/grove_led/
├── GroveLedModule.h    # Public API (correct)
└── RgbInputView.h      # Internal view (should be in src/)
```

**Usage in module:**
```cpp
// From components/grove_led/src/GroveLedModule.cpp (line 2):
#include "grove_led/RgbInputView.h"

// From components/grove_led/src/GroveLedModule.cpp (line 321):
s_rgbInput = new RgbInputView();
```

**View inherits from cdc_ui::ViewBase:**
```cpp
// From components/grove_led/include/grove_led/RgbInputView.h (line 18):
class RgbInputView : public ui::ViewBase {
```

## Recommended Fix

1. **Move view to src/ directory**:
   ```
   components/grove_led/
   ├── include/grove_led/
   │   └── GroveLedModule.h     # Only public header
   └── src/
       ├── GroveLedModule.cpp
       └── RgbInputView.h       # Move here (internal)
           RgbInputView.cpp
   ```

2. **Update includes**:
   ```cpp
   // In src/GroveLedModule.cpp:
   #include "GroveLedModule.h"
   #include "RgbInputView.h"    // Relative to src/
   ```

3. **Consider refactoring**:
   - If `RgbInputView` is generic enough, move to `cdc_views/` as a reusable component
   - If it's grove_led-specific, keep it internal to the module

4. **Update CMakeLists.txt**:
   ```cmake
   # Ensure only include/grove_led/ is in public INCLUDE_DIRS
   # src/ files are compiled but not exposed
   ```

## References

- Module Architecture documentation: `CLAUDE.md` - "All module-related content MUST be placed inside the module itself"
- View pattern: `components/cdc_ui/include/cdc_ui/IView.h`, `ViewStack.h`
- Reusable views: `components/cdc_views/include/cdc_views/` (ListView, SliderView, etc.)
