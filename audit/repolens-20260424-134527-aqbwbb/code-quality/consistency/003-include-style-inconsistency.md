---
title: "[MEDIUM] Include Style Inconsistency: Angle Brackets vs Double Quotes"
severity: MEDIUM
domain: code-structure
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase shows inconsistent include style, mixing angle brackets `< >` and double quotes `" "` for local project headers.

### Evidence

**Angle brackets for local headers (incorrect):**
```cpp
// components/mod_totp/src/TotpModule.cpp
#include <goodisplay/gdey029T94.h>  // Local CalEPD header

// components/mod_gpg/src/openpgp/openpgp.cpp
#include <esp_log.h>  // ESP-IDF header (OK)
```

**Double quotes for local headers (correct):**
```cpp
// components/mod_totp/src/TotpModule.cpp
#include "mod_totp/TotpModule.h"
#include "mod_totp/TotpStore.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_ui/I18n.h"

// components/cdc_core/src/AttestationKeyService.cpp
#include "cdc_core/AttestationKeyService.h"
#include "cdc_log.h"
```

**CalEPD uses angle brackets extensively:**
```cpp
// components/CalEPD/include/epd.h
#include <calepd_version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <epdspi.h>  // Local header!
#include <Adafruit_GFX.h>
```

**Standard headers (should use angle brackets):**
```cpp
// components/mod_totp/src/TotpModule.cpp
#include <cctype>
#include <cstring>
#include <new>
```

## Impact
- **Confusion**: Developers unsure which style to use
- **Include path issues**: Angle brackets search different paths than quotes
- **Maintainability**: Harder to distinguish local vs. external headers at a glance

## Recommended Fix
1. **Standard convention**: Use `" "` for all project-local headers
2. **Angle brackets** for system/external headers only
3. **Fix CalEPD includes**: Change `<epdspi.h>` to `"epdspi.h"` etc.

### Files to fix:
- `components/CalEPD/include/epd.h` and other CalEPD headers
- `components/mod_totp/src/TotpModule.cpp` (fix `<goodisplay/gdey029T94.h>`)

## References
- C++ style guides: Quotes for local, angle brackets for system headers
- ESP-IDF convention: Quotes for project headers, angle for system
