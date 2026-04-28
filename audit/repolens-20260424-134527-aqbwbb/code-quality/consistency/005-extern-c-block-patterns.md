---
title: "[LOW] Inconsistent extern \"C\" block usage in C-style headers"
severity: LOW
domain: code-quality/consistency
lens: header-organization
labels:
  - "audit:code-quality/consistency"
---

## Summary
C-style headers in the FIDO2 module use `extern "C"` blocks, but the pattern is inconsistent with other modules and within the same module itself.

## Impact
- **C++ interoperability**: `extern "C"` is needed for C files to include C++ headers
- **Cognitive overhead**: Mixed patterns create confusion
- **Documentation**: No clear convention for when to use `extern "C"`

## Evidence
**Pattern A - extern "C" block in header** (FIDO2 C-style headers):
```cpp
// components/mod_fido2/include/mod_fido2/fido2.h
#ifndef FIDO2_H
#define FIDO2_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations...

#ifdef __cplusplus
}
#endif

#endif
```

```cpp
// components/mod_fido2/include/mod_fido2/ctap2.h
#ifndef CTAP2_H
#define CTAP2_H

#include <stdint.h>
#include <stdbool.h>
#include "fido2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations...

#ifdef __cplusplus
}
#endif

#endif
```

**Pattern B - Single extern "C" at end** (Module headers):
```cpp
// components/mod_fido2/include/mod_fido2/Fido2Module.h
#pragma once
#include "cdc_core/IModule.h"

namespace cdc::mod_fido2 {
class Fido2Module { ... };
}

extern "C" void mod_fido2_register();  // No block, just single declaration
```

**Pattern C - No extern "C" needed** (Pure C++ headers):
```cpp
// components/mod_gpg/include/mod_gpg/GpgStorage.h
#pragma once
#include <cstdint>
#include <cstddef>

namespace cdc::mod_gpg {
class GpgStorage { ... };
}
```

Inconsistencies:
1. FIDO2 C-style headers use full `extern "C"` blocks
2. Module registration functions use single `extern "C"` declarations
3. Some headers in mod_fido2 have `extern "C"` blocks, others don't
4. No clear documentation on when to use which pattern

## Recommended Fix
**Option 1: Standardize on `extern "C"` blocks for all C-style APIs**

Define "C-style" as headers with C functions (not classes):
```cpp
// For all C-style headers (functions, not methods):
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations...

#ifdef __cplusplus
}
#endif
```

**Option 2: Remove extern "C" blocks, use single declarations**

Only wrap individual functions:
```cpp
// C-style header:
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void fido2_init(void);
uint8_t fido2_get_slot_count(void);

#ifdef __cplusplus
}
#endif
```

**Option 3: Document the convention**

Create a style guide comment in a common header:
```cpp
// components/mod_fido2/include/mod_fido2/fido2.h
/**
 * \file fido2.h
 * \brief FIDO2 C API
 * 
 * Convention: All C-style headers use extern "C" blocks for C++ compatibility.
 * Module headers use single extern "C" for registration functions.
 */
```

## References
- C++ Core Guidelines for C compatibility
- ESP-IDF component structure conventions
- Existing patterns in libtropic_sdk (third_party C library)
