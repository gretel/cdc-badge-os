---
title: "[LOW] Include Path Inconsistency: Mixed include styles in mod_gpg"
severity: LOW
domain: code-style
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "includes"
---

## Summary

The mod_gpg component shows inconsistent include styles for internal headers:

**Variations found:**
1. Angle brackets `<...>`: `#include <esp_log.h>`
2. Double quotes `"..."`: `#include "cdc_log.h"`
3. Relative paths: `#include "openpgp/openpgp.h"`
4. Module namespace: `#include "mod_gpg/GpgModule.h"`

**Evidence:**
```cpp
// components/mod_gpg/src/GpgModule.cpp
#include "mod_gpg/GpgModule.h"
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_log.h"

// components/mod_gpg/src/openpgp/openpgp.cpp
#include <esp_log.h>  // Angle brackets for ESP-IDF
#include "openpgp/openpgp.h"  // Relative path
#include "mod_gpg/GpgStorage.h"  // Module namespace

// components/mod_gpg/src/ccid/ccid_driver.cpp
#include "ccid/ccid_driver.h"  // Relative path
```

## Impact

1. **Search confusion**: IDEs may not find headers with inconsistent patterns
2. **Build portability**: Relative paths can break when files move
3. **Readability**: Harder to quickly identify internal vs external includes

## Evidence

**Include patterns in mod_gpg/src/openpgp/openpgp.cpp:**
```cpp
#include <esp_log.h>
#include "openpgp/openpgp.h"
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_log.h"
```

**Include patterns in mod_gpg/src/GpgModule.cpp:**
```cpp
#include "mod_gpg/GpgModule.h"
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/ModuleRegistry.h"
#include "mod_gpg/gpg.h"
#include "cdc_hal/ISecureElement.h"
```

## Recommended Fix

**Establish include convention:**

1. **External headers** (ESP-IDF): Angle brackets
   ```cpp
   #include <esp_log.h>
   #include <nvs.h>
   ```

2. **Internal module headers**: Module namespace
   ```cpp
   #include "mod_gpg/GpgModule.h"
   #include "mod_gpg/openpgp.h"
   ```

3. **Core headers**: Module namespace
   ```cpp
   #include "cdc_core/ModuleRegistry.h"
   #include "cdc_hal/ISecureElement.h"
   ```

**Fix script:**
```bash
# In mod_gpg/src/openpgp/
sed -i 's/#include "openpgp\//#include "mod_gpg\//g' *.cpp
sed -i 's/#include "ccid\//#include "mod_gpg\//g' ccid/*.cpp
```

## References

- `components/mod_gpg/include/mod_gpg/` - Header structure
- `components/cdc_core/include/cdc_core/` - Core headers
