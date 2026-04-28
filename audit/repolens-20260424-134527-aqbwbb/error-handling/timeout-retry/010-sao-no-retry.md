---
title: "[LOW] SAO EEPROM reads lack retry logic for multi-byte descriptor parsing"
severity: LOW
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "sao"
  - "eeprom"
  - "i2c"
  - "retry"
---

## Summary
The SAO (Standardized Add-On) module (`components/mod_sao/src/sao.cpp`) performs sequential I2C reads to parse the EEPROM descriptor without retry logic. A single transient I2C failure during the multi-byte read sequence causes the entire descriptor parse to fail.

**Evidence:**

1. File: `components/mod_sao/src/sao.cpp:25-28` - EEPROM read helper with no retry:
   ```cpp
   static esp_err_t sao_read_eeprom(uint8_t mem_addr, uint8_t* data, size_t len) {
       if (!g_sao_bus || !g_sao_dev) return ESP_FAIL;
       return g_sao_bus->readReg(g_sao_dev, mem_addr, data, len);  // Single attempt!
   }
   ```

2. File: `components/mod_sao/src/sao.cpp:38-120` - Multi-step descriptor parsing:
   ```cpp
   static bool sao_parse_descriptor(sao_descriptor_t* desc) {
       // 1. Read magic bytes (4 bytes)
       if (sao_read_eeprom(0x00, buf, 4) != ESP_OK) {
           LOG_D(TAG, "Failed to read magic bytes");
           return false;  // Fail on first error
       }
       
       // 2. Read name length and name (variable, multiple reads)
       if (sao_read_eeprom(offset, &name_len, 1) != ESP_OK) return false;
       if (sao_read_eeprom(offset, desc->name, name_len) != ESP_OK) return false;
       
       // 3. Primary driver name (variable, multiple reads)
       if (sao_read_eeprom(offset, &drv_name_len, 1) != ESP_OK) return false;
       if (sao_read_eeprom(offset, desc->primary_driver.name, drv_name_len) != ESP_OK) return false;
       
       // 4. Primary driver data (variable, multiple reads)
       // ... 5+ more sequential reads ...
       
       // 5. Extra drivers (variable, multiple reads per driver)
       // ... potentially 10+ more sequential reads ...
       
       return true;
   }
   ```

3. File: `components/mod_sao/src/sao.cpp:154-172` - Scan calls parse without retry:
   ```cpp
   bool sao_scan(void) {
       // ...
       if (sao_parse_descriptor(&g_sao_descriptor)) {
           g_sao_detected = true;
           return true;
       }
       LOG_D(TAG, "No SAO detected");
       return false;  // No retry on parse failure
   }
   ```

## Impact
- **False negatives**: Transient I2C glitches can cause valid SAO modules to appear undetected
- **Poor UX**: User must physically reconnect module when I2C bus had a temporary glitch
- **Debug difficulty**: Intermittent SAO detection is hard to reproduce and debug
- **Inconsistent behavior**: Same module may work on one scan and fail on the next

## Recommended Fix
Add retry logic to SAO EEPROM reads:

1. Add retry constants:
   ```cpp
   static constexpr uint8_t SAO_EEPROM_RETRY_COUNT = 2;
   static constexpr uint32_t SAO_EEPROM_RETRY_DELAY_MS = 10;
   ```

2. Add retry wrapper:
   ```cpp
   static esp_err_t sao_read_eeprom_with_retry(uint8_t mem_addr, uint8_t* data, size_t len,
                                                uint8_t maxRetries = SAO_EEPROM_RETRY_COUNT) {
       if (!g_sao_bus || !g_sao_dev) return ESP_FAIL;
       
       for (uint8_t i = 0; i < maxRetries; i++) {
           esp_err_t err = g_sao_bus->readReg(g_sao_dev, mem_addr, data, len);
           if (err == ESP_OK) {
               return ESP_OK;
           }
           if (i < maxRetries - 1) {
               vTaskDelay(pdMS_TO_TICKS(SAO_EEPROM_RETRY_DELAY_MS));
           }
       }
       return ESP_FAIL;
   }
   ```

3. Replace reads in parse function:
   ```cpp
   static bool sao_parse_descriptor(sao_descriptor_t* desc) {
       if (!desc) return false;
       memset(desc, 0, sizeof(sao_descriptor_t));

       // 1. Read magic bytes with retry
       uint8_t buf[4] = {0};
       if (sao_read_eeprom_with_retry(0x00, buf, 4) != ESP_OK) {
           LOG_D(TAG, "Failed to read magic bytes");
           return false;
       }

       // 2. Read SAO name length and name with retry
       uint8_t offset = 4;
       uint8_t name_len = 0;
       if (sao_read_eeprom_with_retry(offset, &name_len, 1) != ESP_OK) return false;
       offset += 1;
       if (name_len > SAO_MAX_NAME_LEN) name_len = SAO_MAX_NAME_LEN;
       if (name_len > 0) {
           if (sao_read_eeprom_with_retry(offset, reinterpret_cast<uint8_t*>(desc->name), name_len) != ESP_OK) return false;
           desc->name[name_len] = '\0';
           offset += name_len;
       }

       // ... continue with same pattern for all reads ...

       return true;
   }
   ```

4. Consider adding a "parse attempt" counter with max retries:
   ```cpp
   bool sao_scan(void) {
       if (!g_sao_initialized) {
           if (!sao_init()) return false;
       }

       g_sao_detected = false;
       memset(&g_sao_descriptor, 0, sizeof(g_sao_descriptor));

       // Try parsing up to 3 times with small delay
       for (uint8_t attempt = 0; attempt < 3; attempt++) {
           if (sao_parse_descriptor(&g_sao_descriptor)) {
               g_sao_detected = true;
               LOG_I(TAG, "SAO detected: %s (driver: %s)",
                     g_sao_descriptor.name,
                     g_sao_descriptor.primary_driver.name);
               return true;
           }
           if (attempt < 2) {
               vTaskDelay(pdMS_TO_TICKS(20));
           }
       }

       LOG_D(TAG, "No SAO detected after 3 attempts");
       return false;
   }
   ```

## References
- I2C EEPROM timing considerations: https://ww1.microchip.com/downloads/en/DeviceDoc/22103a.pdf
- ESP32 I2C troubleshooting: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- SAO specification: https://badge.team/docs/hardware/sao/

</content>