---
title: "[MEDIUM] No comprehensive component health status command"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The `STATUS` command in `components/serial_cmd/src/SerialCmd.cpp` (lines 427-434) provides only basic system metrics (free heap, uptime) but does not report the health status of critical hardware components.

**Current implementation:**
```cpp
static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    Console::flush();
}
```

Missing component health checks:
- **Secure Element (TROPIC01)**: Session status, chip ID, firmware version
- **Power Manager**: Battery percentage, voltage, charging state
- **Display**: Initialization state
- **Keypad**: Initialization state
- **WiFi/Bluetooth Controllers**: Connection state
- **I2C Bus**: Initialization state
- **Sleep Controller**: Active state and interval

## Impact

- Operators cannot quickly diagnose which hardware component might be failing
- Troubleshooting requires running multiple individual commands (TR01_STATUS, TR01_INFO, etc.)
- No single command to get a complete system health overview for automated monitoring scripts

## Evidence

**File:** `components/serial_cmd/src/SerialCmd.cpp:427-434`

**File:** `main/main.cpp:99-200` - Shows all hardware components that should be checked:
- I2C bus initialization (line 108)
- Power Manager (line 118)
- Sleep Controller (line 138)
- WiFi Controller (line 158)
- Bluetooth Controller (line 168)
- Keypad (line 178)
- Secure Element (line 188)
- Display (line 198)

## Recommended Fix

Enhance the `STATUS` command to include health checks for all critical hardware components:

1. Add a helper function to query each component's health:
   ```cpp
   static void printComponentHealth() {
       // Check I2C bus
       // Check Power Manager (battery %, charging)
       // Check Secure Element (session active, chip ID)
       // Check Display (initialized, dimensions)
       // Check Keypad (initialized)
       // Check WiFi/Bluetooth (connected, signal strength)
       // Check Sleep Controller (active, interval)
   }
   ```

2. Update `cmdStatus()` to call this helper and display a comprehensive health summary

3. Consider adding a health status indicator (OK/WARNING/CRITICAL) for each component

## References

- ESP32-S3 hardware documentation: https://www.espressif.com/en/products/socs/esp32-s3
- TROPIC01 secure element documentation: https://www.microchip.com/en-us/product/tropic01
