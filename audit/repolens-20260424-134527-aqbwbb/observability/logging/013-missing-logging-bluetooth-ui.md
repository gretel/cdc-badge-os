---
title: "[LOW] Missing logging in BluetoothMenuUi UI flow"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The Bluetooth menu UI (`components/cdc_os_ui/src/BluetoothMenuUi.cpp`) has **zero logging statements** despite implementing critical Bluetooth UI flow including BLE enable/disable, status display, and device scanning.

**Key functions without logging:**
- Bluetooth menu initialization
- BLE enable/disable toggle
- Status display logic
- BLE scan start/stop
- Scan result processing

## Impact
- **Debug difficulty**: When Bluetooth UI fails to update or scan, there's no log trail
- **User experience**: Hard to understand why BLE might not connect or scan
- **Inconsistent with project patterns**: Other UI flows (PinChangeView) have logging

## Evidence
`BluetoothMenuUi.cpp` does NOT include `cdc_log.h`:
```cpp
// Line 1-13: Includes in BluetoothMenuUi.cpp
#include "AppUiInternal.h"
#include "cdc_hal/IBluetoothController.h"
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
// NO cdc_log.h include!
```

## Recommended Fix
Add logging to key Bluetooth UI functions in `BluetoothMenuUi.cpp`:

1. **Add include** (line 1):
```cpp
#include "cdc_log.h"
```

2. **Add TAG definition** (after includes):
```cpp
static const char* TAG = "BtUi";
```

3. **Add logging for BLE toggle events**:
```cpp
// When enabling BLE
LOG_I(TAG, "Enabling Bluetooth...");
// ... enable logic ...
LOG_I(TAG, "Bluetooth enabled");

// When disabling BLE
LOG_I(TAG, "Disabling Bluetooth...");
// ... disable logic ...
LOG_I(TAG, "Bluetooth disabled");
```

4. **Add logging for scan events**:
```cpp
LOG_I(TAG, "Starting BLE scan...");
// ... scan start logic ...
LOG_I(TAG, "BLE scan started");

// When scan completes
LOG_I(TAG, "BLE scan complete, found %d devices", count);
```

5. **Add logging for scan results**:
```cpp
LOG_D(TAG, "Found BLE device: %s (%d dBm)", name, rssi);
```

## References
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp` - Bluetooth UI implementation
- `components/cdc_os_ui/src/views/PinChangeView.cpp` - Example of proper UI flow logging
