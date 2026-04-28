---
title: "[LOW] usb_badge: usb_descriptors.h placed outside include directory"
severity: LOW
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary
The `usb_badge` component has `usb_descriptors.h` placed in the component root (`components/usb_badge/usb_descriptors.h`) instead of in the standard include directory (`components/usb_badge/include/usb_badge/`). This creates an inconsistent structure where the file is accessible but doesn't follow the project's module convention.

**Evidence:**
- **File location**: `components/usb_badge/usb_descriptors.h` (in component root)
- **Standard location**: Should be `components/usb_badge/include/usb_badge/usb_descriptors.h`
- **Other headers follow convention**: `usb_cdc.h`, `usb_hid.h` are properly in `include/usb_badge/`

## Impact
- **Inconsistent structure**: Breaks the project's established module pattern
- **Confusing for developers**: Unclear where to find or place headers
- **Potential include path issues**: May require different include paths depending on context

## Evidence
From the find command results:
```
/components/usb_badge/include/usb_badge/usb_cdc.h      // Correct location
/components/usb_badge/include/usb_badge/usb_hid.h      // Correct location
/components/usb_badge/usb_descriptors.h                // Should be in include/
```

From include grep:
```
/components/usb_badge/usb_hid.cpp:#include "usb_descriptors.h"
```

The file is included without a path prefix because it's in the component root.

## Recommended Fix
1. **Move the header to standard location**:
   - Move `components/usb_badge/usb_descriptors.h` to `components/usb_badge/include/usb_badge/usb_descriptors.h`
   - Move `components/usb_badge/usb_descriptors.h` (the Mac .h file if exists) as well

2. **Update includes**:
   - Change `#include "usb_descriptors.h"` to `#include "usb_badge/usb_descriptors.h"` in:
     - `components/usb_badge/usb_hid.cpp`

3. **Update CMakeLists.txt**:
   - Ensure `include/usb_badge/` is in the INCLUDE_DIRS (should already be there)

## References
- CDC Badge OS Module Architecture documentation in CLAUDE.md
- Project convention: All public headers should be in `include/<component_name>/`
