---
title: "[MEDIUM] Logging TAG Variable Usage Inconsistency"
severity: MEDIUM
domain: logging
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent patterns for logging TAG:

1. **Most modules**: Define `static const char* TAG` and use `LOG_I(TAG, ...)`
2. **mod_fido2 (and others)**: Use string literals directly `LOG_I("FIDO2", ...)`

### Evidence

**mod_fido2 (mostly string literals):**

```cpp
// components/mod_fido2/src/fido2.cpp
LOG_I("FIDO2", "Processing task started");
LOG_W("FIDO2", "USB FIDO write failed");
LOG_D("FIDO2", "Sent response packet");
LOG_I("FIDO2", "Initializing...");
LOG_E("FIDO2", "CTAP2 init failed");
// ... 243 occurrences of string literals
```

```cpp
// components/mod_fido2/src/Fido2Module.cpp
static const char* TAG = "FIDO2";
LOG_I(TAG, "FIDO2 module initialized");
// ... only 13 occurrences using TAG variable
```

**Other modules (consistent TAG usage):**

```cpp
// components/mod_totp/src/TotpModule.cpp
static const char* TAG = "TOTP";
LOG_I(TAG, "Initializing TOTP module");
LOG_E(TAG, "Failed to register i18n strings");

// components/cdc_hal/src/TCA9535Keypad.cpp
static const char* TAG = "Keypad";
LOG_I(TAG, "TCA9535 keypad initialized");

// components/cdc_core/src/ServiceRegistry.cpp
static const char* TAG = "ServiceRegistry";
LOG_I(TAG, "Service registry initialized");
```

**Key differences:**
- **String literal approach**: Hard-coded string in every LOG call
- **TAG variable approach**: Single definition, consistent usage
- **String literal**: More verbose, harder to change log tag
- **TAG variable**: More maintainable, consistent across file

## Impact
- **Maintainability**: Changing log tag requires editing every LOG call
- **Consistency**: Mixed styles make code harder to read
- **Refactoring**: Harder to find all LOG calls for a component
- **Debugging**: TAG variable allows easier log filtering

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt TAG variable approach** (consistent with most modules):
   - Define `static const char* TAG = "ComponentName";` at file top
   - Use `LOG_I(TAG, ...)` consistently

2. **Files to fix (scope for ~1 hour fix):**
   - `components/mod_fido2/src/fido2.cpp` (243 occurrences)
   - Or split into multiple issues if needed

### Rename pattern:

```cpp
// BEFORE (string literal)
#include "mod_fido2/fido2.h"
#include "cdc_log.h"

void fido2_init() {
    LOG_I("FIDO2", "Initializing...");
    LOG_D("FIDO2", "Processing task started");
}

// AFTER (TAG variable)
#include "mod_fido2/fido2.h"
#include "cdc_log.h"

static const char* TAG = "FIDO2";

void fido2_init() {
    LOG_I(TAG, "Initializing...");
    LOG_D(TAG, "Processing task started");
}
```

## References
- Project docs: "ALWAYS use the cdc_log library"
- `components/cdc_log/src/cdc_log.cpp` - Logging implementation (uses TAG pattern)
- Most cdc modules follow TAG variable convention

</content>