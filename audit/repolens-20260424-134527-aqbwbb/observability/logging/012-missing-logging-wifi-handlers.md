---
title: "[LOW] Missing logging in WifiHandlers WiFi wizard"
severity: LOW
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The WiFi handlers component (`components/cdc_os_ui/src/WifiHandlers.cpp`) has **zero logging statements** despite implementing critical WiFi wizard logic including network selection, IP configuration, and time synchronization.

**Key functions without logging:**
- `WifiWizard::reset()` - State reset (line 17)
- `WifiHandlers::isValidIpOctet()` - IP validation (line 37)
- `WifiHandlers::isValidIpAddress()` - IP validation (line 45)
- `WifiHandlers::validateTimeServer()` - NTP server validation
- WiFi connection logic - Network association
- Time sync logic - SNTP synchronization

## Impact
- **Debug difficulty**: When WiFi wizard fails to connect, there's no log trail to diagnose
- **User experience**: Hard to understand why certain networks fail to connect
- **Time sync issues**: No log showing when time synchronization succeeds/fails
- **Inconsistent with project patterns**: Other handlers (SettingsHandlers) have logging

## Evidence
`WifiHandlers.cpp` does NOT include `cdc_log.h`:
```cpp
// Line 1-10: Includes in WifiHandlers.cpp
#include "cdc_os_ui/WifiHandlers.h"
#include "cdc_hal/IWifiController.h"
#include "cdc_hal/IRtc.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>
// NO cdc_log.h include!
```

Compare with SettingsHandlers which has logging:
```cpp
// SettingsHandlers.cpp - has logging
LOG_I(TAG, "Brightness changed to %u", brightness);
LOG_I(TAG, "Language changed to %s", lang);
```

WiFi wizard key functions (lines 17-100) have no logging:
```cpp
void WifiWizard::reset() {
    memset(this, 0, sizeof(*));
    useDhcp = true;
    strncpy(netmask, "255.255.255.0", sizeof(netmask));
    // No log entry!
}
```

## Recommended Fix
Add logging to key WiFi wizard functions in `WifiHandlers.cpp`:

1. **Add includes** (line 1):
```cpp
#include "cdc_log.h"
```

2. **Add TAG definition** (after includes):
```cpp
static const char* TAG = "WifiUi";
```

3. **In `WifiWizard::reset()`** (line 17):
```cpp
void WifiWizard::reset() {
    LOG_D(TAG, "Resetting WiFi wizard state");
    memset(this, 0, sizeof(*this));
    useDhcp = true;
    strncpy(netmask, "255.255.255.0", sizeof(netmask));
}
```

4. **In `isValidIpAddress()`** (line 45):
```cpp
bool WifiHandlers::isValidIpAddress(const char* ip) {
    if (!ip || !ip[0]) {
        LOG_D(TAG, "Empty IP address");
        return false;
    }
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        LOG_D(TAG, "Invalid IP format: %s", ip);
        return false;
    }
    // ...
}
```

5. **Add logging for WiFi connection events** (where WiFi connection logic is called):
```cpp
LOG_I(TAG, "Connecting to WiFi: %s", ssid);
// ... connection logic ...
LOG_I(TAG, "WiFi connected: %s", ip);
```

6. **Add logging for time sync** (where SNTP is configured):
```cpp
LOG_I(TAG, "Configuring SNTP with server: %s", timeServer);
// ... SNTP config ...
LOG_I(TAG, "Time synchronized: %s", timeStr);
```

## References
- `components/cdc_os_ui/src/WifiHandlers.cpp` - WiFi wizard implementation
- `components/cdc_os_ui/src/SettingsHandlers.cpp` - Example of proper handler logging
