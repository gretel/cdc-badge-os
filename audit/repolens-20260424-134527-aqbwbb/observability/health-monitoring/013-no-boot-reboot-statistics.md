---
title: "[LOW] No boot/reboot statistics for reliability tracking"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The system does not track boot/reboot statistics that would be useful for:
- Measuring system stability (reboots per day/week)
- Identifying crash patterns (frequent reboots after certain events)
- Debugging intermittent issues (correlating reboots with time of day, usage patterns)

**What exists:**
- `esp_sleep_wakeup_cause_t` is read at boot (`main/main.cpp:101`) to detect deep-sleep wakeups
- Error log exists but is volatile (PSRAM only, lost on reboot)

**What's missing:**
- No persistent boot counter (total boots since first use)
- No reboot reason tracking (power-on, watchdog, software reset, deep-sleep, brownout)
- No uptime statistics (average uptime before reboot, longest uptime)
- No crash tracking (watchdog resets, panic errors)

## Impact

1. **Blind to stability issues**: Cannot tell if the device is rebooting frequently
2. **Debugging difficulty**: Cannot correlate issues with reboot patterns
3. **No reliability metrics**: Cannot calculate MTBF (Mean Time Between Failures)
4. **Lost crash context**: Error log is volatile - crash info lost on next reboot

## Evidence

**Boot sequence** (`main/main.cpp:101-102`):
```cpp
esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
    LOG_I(TAG, "Woke from deep sleep");
}
```
Only checks for deep-sleep wake, doesn't track other reset reasons.

**Error log** (`components/cdc_log/src/cdc_log.cpp`):
```cpp
EXT_RAM_BSS_ATTR static error_log_entry_t s_error_log[ERROR_LOG_MAX_ENTRIES];
static size_t s_error_log_count = 0;
```
Stored in PSRAM (volatile) - lost on every reboot.

**Reset reason check** (ESP32-S3 has `esp_reset_reason()` but not used):
```bash
grep -r "esp_reset_reason" /input/20260423-132359-oj8ayc/cdc-badge-os/components
# No results
```

## Recommended Fix

Add persistent boot statistics tracking:

**Step 1: Define boot stats structure:**
```cpp
// components/cdc_core/include/cdc_core/BootStats.h
struct BootStats {
    uint32_t total_boots;        // Total number of boots
    uint32_t total_uptime_ms;    // Cumulative uptime
    uint32_t last_reset_reason;  // Last reset reason
    uint64_t last_boot_time;     // Timestamp of last boot
};
```

**Step 2: Implement BootStats service:**
```cpp
// components/cdc_core/src/BootStats.cpp
class BootStats {
    static constexpr const char* NVS_NAMESPACE = "boot_stats";
    static constexpr const char* NVS_KEY = "stats";
    
    BootStats load();
    void save();
    void incrementBoots();
    void recordUptime(uint32_t ms);
    
    BootStats stats_;
    uint32_t session_start_ms_;
};

extern "C" void app_main(void) {
    // At boot
    BootStats::instance().incrementBoots();
    BootStats::instance().recordResetReason(esp_reset_reason());
    BootStats::instance().startSession();
    
    // At shutdown (in main loop or sleep)
    BootStats::instance().recordUptime(esp_timer_get_time() / 1000);
}
```

**Step 3: Add BOOTSTATS command:**
```cpp
static void cmdBootstats(const char* args) {
    BootStats stats = BootStats::instance().get();
    Console::printf("=== Boot Statistics ===\r\n");
    Console::printf("Total boots: %lu\r\n", (unsigned long)stats.total_boots);
    Console::printf("Total uptime: %lu hours\r\n", 
        (unsigned long)(stats.total_uptime_ms / 3600000));
    Console::printf("Last reset: %s\r\n", resetReasonToString(stats.last_reset_reason));
    Console::printf("Session uptime: %lu hours\r\n", 
        (unsigned long)(BootStats::instance().getSessionUptime() / 3600000));
}
```

**Step 4: Include in STATUS command:**
```
=== System Status ===
Version: v0.5
Free heap: 245632 bytes
Uptime: 12345 ms
Boots: 42 (this session)
Last reset: Power-on
```

**Expected output:**
```
=== Boot Statistics ===
Total boots: 156
Total uptime: 432 hours
Last reset: Watchdog timeout
Session uptime: 24 hours
```

## References

- ESP32 reset reasons: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html#reset-reasons
- MTBF calculation: https://www.reliabilitywiki.com/wiki/MTBF_Calculation
- NVS storage: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html

</content>