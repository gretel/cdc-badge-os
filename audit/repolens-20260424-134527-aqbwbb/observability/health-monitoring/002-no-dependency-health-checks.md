---
title: "[MEDIUM] No deep dependency health checks (storage cache, secure element session)"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The `STATUS` command does not verify the operational state of critical dependencies:
1. **TROPIC Storage Cache**: No check if the cache is properly initialized and synchronized
2. **Secure Element Session**: No check if the session is active and responsive
3. **NVS (Non-Volatile Storage)**: No check if NVS is readable/writable

**Current STATUS implementation** (`components/serial_cmd/src/SerialCmd.cpp:427-434`) only reports memory and uptime, not dependency health.

## Impact

- **Silent failures**: A degraded storage cache or expired secure element session might not be immediately visible
- **Delayed detection**: Issues with dependencies are only discovered when a specific module command fails
- **No automated monitoring**: Scripts cannot easily check if all dependencies are healthy

## Evidence

**File:** `components/serial_cmd/src/SerialCmd.cpp:427-434`

**File:** `components/cdc_core/include/cdc_core/TropicStorage.h` - Storage cache service:
- `init()` - Initializes the cache
- `start()` - Starts background operations
- `rebuild()` - Rebuilds cache from secure element

**File:** `components/cdc_hal/include/cdc_hal/ISecureElement.h` - Secure element interface:
- `isSessionActive()` - Returns session state
- `sessionStart()` / `sessionEnd()` - Session management

## Recommended Fix

Add dependency health checks to the `STATUS` command:

1. **Check TROPIC Storage Cache:**
   ```cpp
   auto& storage = core::TropicStorage::instance();
   // Check if cache is initialized and has valid entries
   // Report cache size, last rebuild time, entry count
   ```

2. **Check Secure Element Session:**
   ```cpp
   auto* se = hal::getSecureElementInstance();
   Console::printf("  Session: %s\r\n", se->isSessionActive() ? "active" : "inactive");
   ```

3. **Check NVS Health:**
   ```cpp
   // Attempt a simple read to verify NVS is accessible
   // Report if NVS is healthy or needs attention
   ```

4. **Add a `DEPS` command** for detailed dependency status:
   ```
   DEPS - Show all dependency health checks
   ```

## References

- TROPIC01 Secure Element: https://www.microchip.com/en-us/product/tropic01
- ESP32 NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
