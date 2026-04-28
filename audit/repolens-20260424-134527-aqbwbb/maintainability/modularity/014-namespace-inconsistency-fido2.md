---
title: "[MEDIUM] Missing public API boundaries in mod_fido2 utility modules"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
Several utility files in the `mod_fido2` module expose internal implementation details through C-style `extern "C"` APIs instead of wrapping them in the `cdc::mod_fido2` namespace. This creates an inconsistent public API surface and makes it unclear which functions are meant to be module-internal vs. module-public.

**Locations:**
- `components/mod_fido2/src/pin_storage.cpp` - C-style API, no namespace
- `components/mod_fido2/src/cbor_helpers.cpp` - C-style API, no namespace  
- `components/mod_fido2/src/u2f.cpp` - C-style API, no namespace
- `components/mod_fido2/include/mod_fido2/pin_storage.h` - C-style header
- `components/mod_fido2/include/mod_fido2/cbor_helpers.h` - Mixed (namespace in `#ifdef __DOXYGEN__` only)
- `components/mod_fido2/include/mod_fido2/u2f.h` - C-style header

## Impact
**API Boundary Confusion:**
- Some files use `cdc::mod_fido2` namespace (`Fido2Module.cpp`, `Fido2Ui.cpp`, `ctap2.cpp`, `fido2.cpp`, `fido2_storage.cpp`, `ctaphid.cpp`)
- Others use C-style global functions (`pin_storage.cpp`, `cbor_helpers.cpp`, `u2f.cpp`)
- Makes it unclear which convention to follow when adding new code

**Discoverability:**
- IDE autocomplete doesn't work consistently across the module
- Developers may miss functions that are in the global namespace
- Harder to understand the module's public API surface

**Maintenance:**
- Two different calling conventions within the same module
- Risk of accidental global namespace pollution
- Unclear which functions are intended as public API vs. internal helpers

## Evidence
**Files using `cdc::mod_fido2` namespace:**
```cpp
// Fido2Module.cpp:17
namespace cdc::mod_fido2 {
// ...
} // namespace cdc::mod_fido2

// fido2_storage.cpp:26
namespace cdc::mod_fido2 {
// ...
} // namespace cdc::mod_fido2
```

**Files using C-style global functions (no namespace):**
```cpp
// pin_storage.cpp - no namespace wrapper
bool pin_storage_fido2_available(void) {
    // ...
}

// cbor_helpers.cpp - no namespace wrapper
void cbor_writer_init(cbor_writer_t *w, uint8_t *buffer, size_t size) {
    // ...
}

// u2f.cpp - no namespace wrapper
uint8_t u2f_init(void) {
    // ...
}
```

**Header inconsistency:**
```cpp
// cbor_helpers.h - namespace wrapped in __DOXYGEN__ only
#ifdef __DOXYGEN__
namespace cdc::mod_fido2 {
#endif
typedef struct { ... } cbor_writer_t;
void cbor_writer_init(cbor_writer_t *w, uint8_t *buffer, size_t size);
#ifdef __DOXYGEN__
} // namespace cdc::mod_fido2
#endif

// pin_storage.h - pure C-style
#ifdef __cplusplus
extern "C" {
#endif
bool pin_storage_fido2_available(void);
#ifdef __cplusplus
}
#endif
```

## Recommended Fix
Standardize all utility files to use the `cdc::mod_fido2` namespace:

**Step 1: Update pin_storage.h**
```cpp
#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace cdc::mod_fido2 {

bool pin_storage_fido2_available(void);
bool pin_storage_get_fido2_hash(uint8_t* hash_out);
bool pin_storage_verify_fido2_hash(const uint8_t* hash_in);
bool pin_storage_is_set(void);

} // namespace cdc::mod_fido2
```

**Step 2: Update pin_storage.cpp**
```cpp
#include "mod_fido2/pin_storage.h"
#include "cdc_core/PinManager.h"

namespace cdc::mod_fido2 {

bool pin_storage_fido2_available(void) {
    // ...
}

// ... other functions

} // namespace cdc::mod_fido2
```

**Step 3: Update cbor_helpers.h**
Remove the `#ifdef __DOXYGEN__` wrapper and use proper namespace:
```cpp
#ifndef CBOR_HELPERS_H
#define CBOR_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace cdc::mod_fido2 {

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t offset;
    bool error;
} cbor_writer_t;

void cbor_writer_init(cbor_writer_t *w, uint8_t *buffer, size_t size);
// ... other declarations

} // namespace cdc::mod_fido2

#endif // CBOR_HELPERS_H
```

**Step 4: Update cbor_helpers.cpp**
```cpp
#include "mod_fido2/cbor_helpers.h"
#include "cdc_log.h"
#include <string.h>

namespace cdc::mod_fido2 {

void cbor_writer_init(cbor_writer_t *w, uint8_t *buffer, size_t size) {
    // ...
}

// ... other functions

} // namespace cdc::mod_fido2
```

**Step 5: Update u2f.h and u2f.cpp** similarly.

**Step 6: Update callers** to use qualified names or `using` declarations:
```cpp
// Before:
cbor_writer_init(&w, buffer, size);

// After (option 1 - qualified):
cdc::mod_fido2::cbor_writer_init(&w, buffer, size);

// After (option 2 - using):
using cdc::mod_fido2::cbor_writer_init;
cbor_writer_init(&w, buffer, size);
```

## References
- C++ namespaces best practices: https://en.wikipedia.org/wiki/Namespace_(C++)
- Module cohesion: https://en.wikipedia.org/wiki/Cohesion_(computer_science)
- Existing project pattern: `mod_gpg`, `mod_totp`, `mod_password` all use consistent namespace wrappers
