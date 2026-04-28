---
title: "[MEDIUM] Hardcoded timeout values scattered across modules"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
Timeout values are hardcoded in multiple files across the codebase without a centralized configuration. This makes tuning timeouts for different environments difficult and increases the risk of inconsistent behavior.

## Impact
1. **Maintenance burden**: Changing timeout behavior requires editing multiple files
2. **Inconsistent UX**: Different modules may use different timeout strategies
3. **Hard to tune**: Cannot adjust timeouts for slow/fast hardware without recompiling
4. **Debug difficulty**: When debugging timeout issues, must search all files

## Evidence
**Timeout values found in different locations:**

| Timeout | Value | Location |
|---------|-------|----------|
| Inactivity timeout | 5 min (300000ms) | `components/cdc_os_ui/src/AppUi.cpp:42` |
| Serial auth timeout | 5 min (300000ms) | `components/serial_cmd/include/serial_cmd/SerialCmd.h:15` |
| WiFi connect timeout | 15000ms | `components/cdc_os_ui/include/cdc_os_ui/WifiHandlers.h:10` |
| WiFi scan timeout | 10000ms | `components/cdc_os_ui/include/cdc_os_ui/WifiHandlers.h:11` |
| NTP sync timeout | 10000ms | `components/cdc_os_ui/include/cdc_os_ui/WifiHandlers.h:12` |
| BLE connect timeout | 10000ms | `components/mod_vcard/src/ble_vcard.cpp:25` |
| BLE discovery timeout | 5000ms | `components/mod_vcard/src/ble_vcard.cpp:26` |
| BLE consent timeout | 30000ms | `components/mod_vcard/src/ble_vcard.cpp:27` |
| BLE operation timeout | 10000ms | `components/mod_vcard/src/ble_vcard.cpp:28` |
| I2C timeout | 100ms | `components/cdc_hal/src/I2cBus.cpp:19` |
| Keypad poll timeout | 50ms | `components/cdc_hal/src/TCA9535Keypad.cpp:20` |
| T9 input timeout | 2000ms | `components/cdc_views/include/cdc_views/T9InputView.h:12` |
| Light sleep timeout | 120s | `components/cdc_os_ui/include/cdc_os_ui/SleepManager.h:10` |
| FIDO2 user presence | 30s | `components/mod_fido2/src/ctap2.cpp:15` |

**Code examples:**
```cpp
// components/cdc_os_ui/src/AppUi.cpp:42
static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;

// components/mod_vcard/src/ble_vcard.cpp:25-28
static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
static constexpr uint32_t DISCOVERY_TIMEOUT_MS = 5000;
static constexpr uint32_t CONSENT_TIMEOUT_MS = 30000;
static constexpr uint32_t OPERATION_TIMEOUT_MS = 10000;

// components/cdc_hal/src/I2cBus.cpp:19
static constexpr uint32_t I2C_TIMEOUT_MS = 100;
```

## Recommended Fix
Create timeout configuration in centralized `Config.h`:

1. **Add timeout configuration to `components/cdc_core/include/cdc_core/Config.h`**:
   ```cpp
   // Timeout Configuration (in milliseconds)
   namespace timeout {
   // UI timeouts
   constexpr uint32_t INACTIVITY_MS = 5 * 60 * 1000;  // 5 minutes
   constexpr uint32_t T9_INPUT_MS = 2000;  // 2 seconds
   
   // Authentication timeouts
   constexpr uint32_t SERIAL_AUTH_MS = 5 * 60 * 1000;  // 5 minutes
   constexpr uint32_t FIDO2_USER_PRESENCE_MS = 30 * 1000;  // 30 seconds
   
   // Network timeouts
   constexpr uint32_t WIFI_CONNECT_MS = 15 * 1000;  // 15 seconds
   constexpr uint32_t WIFI_SCAN_MS = 10 * 1000;  // 10 seconds
   constexpr uint32_t NTP_SYNC_MS = 10 * 1000;  // 10 seconds
   
   // BLE timeouts
   constexpr uint32_t BLE_CONNECT_MS = 10 * 1000;  // 10 seconds
   constexpr uint32_t BLE_DISCOVERY_MS = 5 * 1000;  // 5 seconds
   constexpr uint32_t BLE_CONSENT_MS = 30 * 1000;  // 30 seconds
   constexpr uint32_t BLE_OPERATION_MS = 10 * 1000;  // 10 seconds
   
   // Hardware timeouts
   constexpr uint32_t I2C_MS = 100;  // 100ms
   constexpr uint32_t KEYPAD_POLL_MS = 50;  // 50ms
   
   // Sleep timeouts
   constexpr uint32_t LIGHT_SLEEP_MS = 120 * 1000;  // 120 seconds
   }
   ```

2. **Update each file to use centralized config**:
   ```cpp
   // Instead of:
   static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;
   
   // Use:
   #include "cdc_core/Config.h"
   static constexpr uint32_t INACTIVITY_TIMEOUT_MS = cdc::config::timeout::INACTIVITY_MS;
   ```

## References
- [Configuration Management Best Practices](https://12factor.net/config)
- [Timeout patterns in embedded systems](https://www.embedded.com/design/prototyping-and-system-design/4024921/Timeout-patterns-for-embedded-systems)
