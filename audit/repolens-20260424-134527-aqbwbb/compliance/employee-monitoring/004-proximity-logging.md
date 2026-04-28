---
title: "[LOW] Proximity Data Logged Without Configurable Log Retention"
severity: LOW
domain: employee-monitoring
lens: betriebsrat-compliance
labels:
  - logging
  - data-retention
  - proximity-tracking
---

## Summary

The **mod_vcard** component logs discovered BLE peers with MAC addresses and RSSI values using `LOG_I()` but lacks configurable log retention. Under GDPR and BetrVG, logging of proximity data should have configurable retention periods and the ability to disable logging for privacy.

**Files Affected:**
- `components/mod_vcard/src/ble_vcard.cpp` (line 592)

**Evidence:**

```cpp
// components/mod_vcard/src/ble_vcard.cpp:592
LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);

// This log contains:
// - peer.name: Device name (potentially employee name)
// - peer.rssi: Signal strength (proximity indicator)
// - Implicit: Timestamp (from log system), MAC address (not logged but stored in s_peers)
```

The logging happens every time a new peer is discovered (up to 32 peers in scan results). The log output includes:
- **Device name**: Could contain employee names or location identifiers
- **RSSI value**: Signal strength indicates proximity, can be used for tracking movement

## Impact

Under **GDPR Article 5(1)(e)** (storage limitation) and **BetrVG §87 Abs. 1 Nr. 6** (monitoring systems):

1. **Log Data Accumulation**: Serial logs accumulate proximity data without automatic cleanup
2. **No Log Retention Configuration**: Cannot configure how long proximity logs are kept
3. **No Log Disable Option**: Cannot disable logging for privacy-sensitive scenarios
4. **Potential for Indefinite Storage**: Logs may be captured to USB/serial buffer and stored indefinitely

## Recommended Fix

Implement the following within ~1 hour:

1. **Add log configuration** in `ble_vcard.h`:
   ```cpp
   typedef struct {
       bool log_enabled;           // Enable/disable proximity logging
       uint32_t log_retention_ms;  // How long to keep logs (0 = infinite)
   } vcard_log_config_t;
   
   void ble_vcard_set_log_config(const vcard_log_config_t* config);
   vcard_log_config_t ble_vcard_get_log_config(void);
   ```

2. **Conditional logging** in `ble_vcard.cpp`:
   ```cpp
   static vcard_log_config_t s_log_config = {
       .log_enabled = true,
       .log_retention_ms = 0
   };
   
   static void processScanResults() {
       // ... existing code ...
       
       if (s_log_config.log_enabled) {
           LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);
       }
   }
   ```

3. **Serial command for log management** in `VcardModule.cpp`:
   ```cpp
   static void cmdVcardLog(const char* args) {
       // Usage: VCARD_LOG [on|off] [retention_ms]
       vcard_log_config_t config = ble_vcard_get_log_config();
       
       if (strstr(args, "off")) {
           config.log_enabled = false;
       } else if (strstr(args, "on")) {
           config.log_enabled = true;
       }
       
       // Parse retention if provided
       char* endptr;
       long retention = strtol(args, &endptr, 10);
       if (endptr != args) {
           config.log_retention_ms = retention;
       }
       
       ble_vcard_set_log_config(&config);
       serial::Console::printf("Log: %s, Retention: %lu ms\r\n", 
                               config.log_enabled ? "on" : "off", 
                               config.log_retention_ms);
   }
   
   reg.registerCommand({"VCARD_LOG", "Configure proximity logging", cmdVcardLog, "vcard", false});
   ```

## References

- **GDPR Article 5(1)(e)**: Storage limitation - data kept no longer than necessary
- **BetrVG §87 Abs. 1 Nr. 6**: Works Council co-determination for monitoring
- **BAG Urteil vom 1.12.2015 (Az.: 1 ABR 10/14)**: Logging of proximity data qualifies as monitoring
- **ISO/IEC 27001**: Log retention should be configurable based on business needs
