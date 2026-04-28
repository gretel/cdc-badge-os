---
title: "[MEDIUM] mod_fido2: Fido2Ui.h public header exposes internal fido2.h"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_fido2` module's public header `Fido2Ui.h` includes the internal implementation header `fido2.h`, creating a public API dependency on internal implementation details:

```cpp
// components/mod_fido2/include/mod_fido2/Fido2Ui.h (line 3):
#include "mod_fido2/fido2.h"  // Internal header exposed via public API
```

This means any code including `Fido2Ui.h` (a supposedly public UI interface) also gets all of `fido2.h`'s internal types and definitions, even if they only need the UI functions.

## Impact

- **Unnecessary Coupling**: External consumers of `Fido2Ui.h` get polluted with internal FIDO2 types
- **Compilation Dependencies**: Changes to `fido2.h` force recompilation of all files including `Fido2Ui.h`
- **Type Leakage**: Internal types like `fido2_user_presence_result_t`, `fido2_action_t` become part of public surface
- **Forward Declaration Opportunity Missed**: Could use forward declarations instead of full include

## Evidence

**Current public API header:**
```cpp
// components/mod_fido2/include/mod_fido2/Fido2Ui.h:
#pragma once

#include "mod_fido2/fido2.h"  // ← Internal header exposed!
#include "cdc_ui/IView.h"

namespace cdc::mod_fido2 {

void fido2_ui_init();
cdc::ui::IView* fido2_ui_get_list_view();
const char* fido2_ui_get_label();
fido2_user_presence_result_t fido2_ui_user_presence_callback(
    const char* rp_id,
    fido2_action_t action,
    const char* user_name
);

} // namespace cdc::mod_fido2
```

**Internal header that's exposed:**
```cpp
// components/mod_fido2/include/mod_fido2/fido2.h (internal):
#define FIDO2_MAX_CREDENTIALS 20
#define FIDO2_MAX_ORG_KEYS 10
// ... 50+ lines of internal definitions
```

**Usage pattern showing the problem:**
```cpp
// If external code only wants UI:
#include "mod_fido2/Fido2Ui.h"  // Gets fido2.h too!

// Internal header includes:
// From components/mod_fido2/src/Fido2Ui.cpp:
#include "mod_fido2/Fido2Ui.h"
#include "mod_fido2/fido2.h"     // Redundant - already included
#include "mod_fido2/fido2_storage.h"
```

## Recommended Fix

1. **Replace full include with forward declarations:**
   ```cpp
   // components/mod_fido2/include/mod_fido2/Fido2Ui.h:
   #pragma once
   
   #include "cdc_ui/IView.h"
   
   // Forward declare internal types needed in public API
   namespace cdc::mod_fido2 {
       struct fido2_user_presence_result_t;  // Forward declare
       enum class fido2_action_t;            // Forward declare
   }
   
   namespace cdc::mod_fido2 {
   
   void fido2_ui_init();
   cdc::ui::IView* fido2_ui_get_list_view();
   const char* fido2_ui_get_label();
   fido2_user_presence_result_t fido2_ui_user_presence_callback(
       const char* rp_id,
       fido2_action_t action,
       const char* user_name
   );
   
   } // namespace cdc::mod_fido2
   ```

2. **Or move type definitions to a shared header:**
   ```cpp
   // components/mod_fido2/include/mod_fido2/Fido2Types.h (new public types):
   #pragma once
   
   typedef struct {
       bool user_present;
       bool signature_found;
   } fido2_user_presence_result_t;
   
   typedef enum {
       FIDO2_ACTION_SIGN,
       FIDO2_ACTION_ADD
   } fido2_action_t;
   
   // Then Fido2Ui.h includes only this:
   #include "Fido2Types.h"
   ```

3. **Update Fido2Ui.cpp to include internal header:**
   ```cpp
   // components/mod_fido2/src/Fido2Ui.cpp:
   #include "Fido2Ui.h"           // Public header
   #include "fido2.h"             // Internal (now first include)
   #include "fido2_storage.h"
   ```

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- C++ best practices: Forward declarations to reduce coupling
- Similar pattern: `GpgModule.h` only includes `cdc_core/IModule.h` (clean public API)
