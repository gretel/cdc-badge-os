---
title: "[LOW] Missing logging in ExpertMenuUi module control"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The Expert menu UI (`components/cdc_os_ui/src/ExpertMenuUi.cpp`) has **zero logging statements** despite implementing critical module control actions including module enable/disable, TROPIC maintenance, and hardware info display.

**Key functions without logging:**
- Expert menu initialization
- Module enable/disable actions
- TROPIC storage maintenance
- Hardware info display
- Module view population

## Impact
- **Debug difficulty**: When module control fails, there's no log trail to diagnose
- **User experience**: Hard to understand why module enable/disable might fail
- **Maintenance**: TROPIC maintenance actions leave no audit trail
- **Inconsistent with project patterns**: Other menu UIs (SettingsHandlers) have logging

## Evidence
`ExpertMenuUi.cpp` does NOT include `cdc_log.h`:
```cpp
// Line 1-14: Includes in ExpertMenuUi.cpp
#include "AppUiInternal.h"
#include "cdc_os_ui/AppUi.h"
#include "cdc_os_ui/HardwareInfo.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_core/UsbManager.h"
#include "cdc_core/EventBus.h"
#include <cstdio>
#include <cstring>
// NO cdc_log.h include!
```

## Recommended Fix
Add logging to key Expert menu functions in `ExpertMenuUi.cpp`:

1. **Add include** (line 1):
```cpp
#include "cdc_log.h"
```

2. **Add TAG definition** (after includes):
```cpp
static const char* TAG = "ExpertUi";
```

3. **Add logging for module enable/disable**:
```cpp
// When enabling a module
LOG_I(TAG, "Enabling module: %s", moduleName);
// ... enable logic ...
LOG_I(TAG, "Module enabled: %s", moduleName);

// When disabling a module
LOG_I(TAG, "Disabling module: %s", moduleName);
// ... disable logic ...
LOG_I(TAG, "Module disabled: %s", moduleName);
```

4. **Add logging for TROPIC maintenance**:
```cpp
LOG_I(TAG, "Starting TROPIC maintenance...");
// ... maintenance logic ...
LOG_I(TAG, "TROPIC maintenance complete");
```

5. **Add logging for module view population**:
```cpp
LOG_D(TAG, "Populating module view with %d modules", count);
```

## References
- `components/cdc_os_ui/src/ExpertMenuUi.cpp` - Expert menu implementation
- `components/cdc_os_ui/src/SettingsHandlers.cpp` - Example of proper handler logging
