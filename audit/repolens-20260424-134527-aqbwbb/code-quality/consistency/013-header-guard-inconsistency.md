---
title: "[LOW] Header Guard Style Inconsistency"
severity: LOW
domain: code-structure
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent header guard styles:

1. **cdc_core/cdc_ui/cdc_hal**: `#pragma once` (modern, concise)
2. **CalEPD**: Traditional `#ifndef`/`#define`/`#endif` guards with mixed naming conventions

### Evidence

**cdc_core (pragma once - consistent):**
```cpp
// components/cdc_core/include/cdc_core/ServiceRegistry.h
#pragma once

#include "IService.h"
#include <cstddef>

namespace cdc::core {
// ...
}
```

**CalEPD (traditional guards - inconsistent naming):**
```cpp
// components/CalEPD/include/parallel/ED060SC4.h
#ifndef CALEPD_EXCLUDE_PARALLEL
#define CALEPD_EXCLUDE_PARALLEL

// components/CalEPD/include/epdspi.h
#ifndef epdspi_h
#define epdspi_h

// components/CalEPD/include/plasticlogic.h
#ifndef plasticlogic_h
#define plasticlogic_h

// components/CalEPD/include/epd4spi.h
#ifndef epd4spi_h
#define epd4spi_h

// components/CalEPD/include/epd.h
#ifndef epd_h
#define epd_h

// components/CalEPD/include/iointerface.h
#ifndef iointerface_h
#define iointerface_h
```

**Key inconsistencies:**
- `#pragma once` vs. `#ifndef` guards
- Guard naming: `CALEPD_EXCLUDE_PARALLEL` vs. `epdspi_h` vs. `plasticlogic_h`
- Some guards use uppercase, some use lowercase
- Guards don't consistently match filename

## Impact
- **Modern vs. legacy**: `#pragma once` is more modern and concise
- **Compatibility**: Traditional guards are more portable (some compilers don't support `#pragma once`)
- **Naming convention**: Inconsistent guard names make it harder to find related files
- **Readability**: `#pragma once` is clearer and shorter

## Recommended Fix

**Establish and document a single convention:**

**Option A: Adopt `#pragma once` (modern style)**
- Consistent with cdc_core
- More concise and readable
- Supported by all modern compilers (GCC, Clang, MSVC)

**Option B: Standardize traditional guards**
- Better for maximum portability
- Use consistent naming: `COMPONENT_NAME_FILE_H` (uppercase with underscores)

### Recommended: Option A (`#pragma once`)

**Files to fix in CalEPD (scope for ~1 hour fix):**
- `components/CalEPD/include/epdspi.h`
- `components/CalEPD/include/plasticlogic.h`
- `components/CalEPD/include/epd4spi.h`
- `components/CalEPD/include/epdspi2cs.h`
- `components/CalEPD/include/epd7color.h`
- `components/CalEPD/include/epd.h`
- `components/CalEPD/include/iointerface.h`
- `components/CalEPD/include/parallel/ED060SC4.h`
- `components/CalEPD/include/parallel/ED047TC1touch.h`
- `components/CalEPD/include/parallel/ED047TC1.h`

### Rename pattern:
```cpp
// BEFORE (traditional guard)
#ifndef epdspi_h
#define epdspi_h

// ... header content ...

#endif

// AFTER (pragma once)
#pragma once

// ... header content ...
```

## References
- C++ Core Guidelines: Headers should use `#pragma once`
- ESP-IDF convention: Uses `#pragma once`
- Compiler support: `#pragma once` supported by GCC, Clang, MSVC, ESP-IDF

</content>