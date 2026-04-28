---
title: "[MEDIUM] cdc_views: RenderHelpers.h utility header exposed in public include directory"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `cdc_views` module exposes `RenderHelpers.h`, an internal utility header, in its public include directory (`components/cdc_views/include/cdc_views/`). This header contains rendering helper functions intended for internal use within the views module, but is accessible to all external modules.

**Key observation:** The `cdc_os_ui` module directly includes this internal utility:
```cpp
// From components/cdc_os_ui/src/views/PinChangeView.cpp:
#include "cdc_views/RenderHelpers.h"
```

This creates a dependency on an implementation detail of `cdc_views`.

## Impact

- **Internal API Leakage**: `RenderHelpers.h` is a utility header containing screen drawing helpers, not a primary view component. It's meant to support other views internally.
- **Cross-Module Coupling**: `cdc_os_ui` depends on `cdc_views` internals, making it harder to refactor `cdc_views` without affecting `cdc_os_ui`.
- **Maintenance Burden**: Changes to rendering helpers (like `kFooterHeight`, `drawHeaderLeft`) could break external consumers unexpectedly.
- **No Clear Distinction**: No way for consumers to know which headers are "stable public API" vs "internal utilities".

## Evidence

**Public include directory structure:**
```
components/cdc_views/include/cdc_views/
├── ListView.h          # Public view (correct)
├── PinEntryView.h      # Public view (correct)
├── RenderHelpers.h     # Internal utility (exposed!)
├── ToastView.h         # Public view (correct)
└── ...
```

**Cross-module usage showing the problem:**
```cpp
// components/cdc_os_ui/src/views/PinChangeView.cpp:
#include "cdc_ui/IView.h"
#include "cdc_views/RenderHelpers.h"  // Internal utility from cdc_views
#include <cstdint>
```

**Internal usage within cdc_views:**
```cpp
// RenderHelpers.h is included by many internal views:
// - ToastView.cpp, ListView.cpp, TimeInputView.cpp
// - ContextMenuView.cpp, ConfirmView.cpp, DateInputView.cpp
// - MessageBox.cpp, SliderView.cpp, PinEntryView.cpp
// - T9InputView.cpp, InfoView.cpp
```

**Header content showing internal nature:**
```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h:
namespace cdc::ui::render {
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;

void drawHeaderLeft(Gdey029T94* gfx, const char* title, int x, int y, ...);
void drawFooterBar(Gdey029T94* gfx, uint16_t width, uint16_t height, ...);
// ... rendering helpers, not a view class
}
```

## Recommended Fix

1. **Move RenderHelpers.h to src/ directory:**
   ```
   components/cdc_views/
   ├── include/cdc_views/
   │   └── (only public view headers)
   └── src/
       ├── RenderHelpers.h    # Move here
       └── RenderHelpers.cpp
   ```

2. **Update internal includes:**
   ```cpp
   // In cdc_views/src/*.cpp files, change:
   #include "cdc_views/RenderHelpers.h"  →  #include "RenderHelpers.h"
   ```

3. **Update cross-module usage (cdc_os_ui):**
   ```cpp
   // Option A: Move rendering logic to cdc_os_ui if it's OS-specific
   // Option B: Create a dedicated "ui_utils" component for shared rendering helpers
   // Option C: Include the moved header from src (if acceptable for this case)
   ```

4. **Add documentation to clarify public vs internal:**
   ```cpp
   // Add to cdc_views README or header comments:
   /**
    * cdc_views Public API
    * ====================
    * These headers are stable and can be included by external modules:
    * - ListView.h, PinEntryView.h, SliderView.h, etc. (view classes)
    *
    * Internal headers (in src/) are for module-internal use only.
    */
   ```

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- Similar findings: Issues #001, #004 (internal headers exposed in mod_gpg, mod_fido2)
- C++ best practices: Separate public API from internal utilities

(End of file)
