---
title: "[LOW] `using namespace cdc::ui;` pollutes global scope in WifiMenuUi.cpp"
severity: LOW
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The file `WifiMenuUi.cpp` uses `using namespace cdc::ui;` at file scope, importing all names from the `cdc::ui` namespace into the local scope. This is redundant since the file is already in `namespace cdc::ui { }` and can lead to name collisions and reduced code clarity.

**Evidence:**
- File: `components/cdc_os_ui/src/WifiMenuUi.cpp` (line 20)
- Also found in: `components/mod_nvsedit/src/NvsEditModule.cpp` (line 21)

## Impact
1. **Redundancy**: Already inside `namespace cdc::ui`, so the using directive is unnecessary
2. **Name collision risk**: Could shadow local variables or create ambiguity
3. **Code clarity**: Makes it unclear which names are from the namespace vs local scope
4. **Inconsistent style**: Other files don't use this pattern

## Evidence
**WifiMenuUi.cpp** (lines 16-25):
```cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

namespace cdc::ui {

using namespace cdc::ui;  // Redundant - already in cdc::ui namespace!

/** \brief Wi-Fi menu size limits. */
static constexpr uint8_t WIFI_MAX_NETWORKS = 20;
```

**NvsEditModule.cpp** (lines 18-25):
```cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace cdc::mod_nvsedit {

using namespace cdc::ui;  // Imports cdc::ui into cdc::mod_nvsedit
```

## Recommended Fix
1. Remove `using namespace cdc::ui;` from `WifiMenuUi.cpp` (1 minute)
2. Remove `using namespace cdc::ui;` from `NvsEditModule.cpp` - or keep if truly needed for cross-namespace access (2 minutes)
3. Use explicit namespace qualifiers where needed: `ui::ListView`, `ui::I18n`, etc. (5 minutes)

**Total estimated time: ~10 minutes**

## References
- C++ Core Guidelines [R.11](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-using): Avoid `using namespace`
- Consistency: Other files in the project use explicit namespace qualifiers
