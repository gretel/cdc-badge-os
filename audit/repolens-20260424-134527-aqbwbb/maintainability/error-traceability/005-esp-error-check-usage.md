---
title: "[MEDIUM] ESP_ERROR_CHECK used without error context preservation"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
ESP_ERROR_CHECK is used in several locations, which causes immediate reset on error without preserving context about where the error occurred or what operation was being performed.

## Impact
- **Lost context**: When ESP_ERROR_CHECK triggers, only the error code is known, not the specific operation
- **No error aggregation**: Errors bypass the cdc_log error ring buffer
- **Hard debugging**: Stack trace shows ESP_ERROR_CHECK line, not the actual failing operation
- **No graceful recovery**: System resets instead of logging and continuing

## Evidence

### CalEPD components - Multiple locations:

`components/CalEPD/epdspi.cpp`:
```cpp
esp_err_t ret = spi_master_trans(...);
ESP_ERROR_CHECK(ret);  // No context if this fails
```

`components/CalEPD/epd4spi.cpp`:
```cpp
esp_err_t ret = spi_master_trans(...);
ESP_ERROR_CHECK(ret);  // Called 10+ times throughout file
```

`components/CalEPD/models/plasticlogic/epdspi2cs.cpp`:
```cpp
esp_err_t ret = spi_master_trans(...);
ESP_ERROR_CHECK(ret);  // Called 4 times
```

### Bluetooth Controller:

`components/cdc_hal/src/BluetoothController.cpp`:
```cpp
ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
```

### Pattern analysis:
- Total ESP_ERROR_CHECK occurrences: ~15
- All in CalEPD display drivers (non-critical path)
- One in Bluetooth initialization (could be more impactful)

## Recommended Fix

1. **Replace ESP_ERROR_CHECK with explicit error handling:**

   ```cpp
   // Instead of:
   esp_err_t ret = spi_master_trans(...);
   ESP_ERROR_CHECK(ret);
   
   // Use:
   esp_err_t ret = spi_master_trans(...);
   if (ret != ESP_OK) {
       LOG_E(TAG, "SPI transfer failed at %s:%d: %s", 
             __FILE__, __LINE__, esp_err_to_name(ret));
       return ret;  // Or handle gracefully
   }
   ```

2. **For critical initialization:**

   ```cpp
   // Instead of:
   ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
   
   // Use:
   esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
   if (err != ESP_OK) {
       LOG_E(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(err));
       // Return error or use ESP_ERROR_CHECK_WITHOUT_ABORT for logging
       return false;
   }
   ```

3. **Create helper macro for context-aware error checking:**

   ```cpp
   // components/cdc_core/ErrorHandling.h
   #define CHECK_ERROR(expr) do { \
       esp_err_t _err = (expr); \
       if (_err != ESP_OK) { \
           LOG_E(TAG, "Failed at %s:%d: %s", __func__, __LINE__, esp_err_to_name(_err)); \
           return _err; \
       } \
   } while(0)
   
   // Usage:
   CHECK_ERROR(spi_master_trans(...));
   ```

4. **Prioritize by impact:**
   - High: Bluetooth controller (affects core functionality)
   - Medium: Display drivers (affects UI)

## References
- [ESP-IDF ESP_ERROR_CHECK documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/utils.html#esp-error-check)
- Project logging guide: `CLAUDE.md` - "ALWAYS use the cdc_log library"
