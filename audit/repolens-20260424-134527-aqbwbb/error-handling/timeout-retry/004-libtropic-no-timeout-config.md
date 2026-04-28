---
title: "[MEDIUM] TROPIC01 SPI port ignores configurable timeout parameter"
severity: MEDIUM
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "tropic01"
  - "spi"
  - "timeout"
  - "secure-element"
---

## Summary
The TROPIC01 SPI port implementation (`components/cdc_hal/src/libtropic_port_esp32.cpp`) receives a `timeout_ms` parameter from libtropic but ignores it, using `spi_device_polling_transmit` which blocks indefinitely on SPI bus acquisition.

**Evidence:**
- File: `components/cdc_hal/src/libtropic_port_esp32.cpp:105-129` - Timeout parameter ignored:
  ```cpp
  extern "C" lt_ret_t lt_port_spi_transfer(lt_l2_state_t *s2, uint8_t offset, uint16_t tx_len,
                                           uint32_t timeout_ms) {
      (void)timeout_ms;  // <-- Explicitly cast to void (ignored)
      if (!s2 || !s2->device) {
          return LT_PARAM_ERR;
      }
      lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
      
      // In-place SPI transfer (TX/RX share buffer)
      spi_transaction_t t = {};
      t.length = static_cast<size_t>(tx_len) * 8;
      t.tx_buffer = s2->buff + offset;
      t.rx_buffer = s2->buff + offset;
      esp_err_t err = spi_device_polling_transmit(device->spi, &t);  // No timeout!
      
      if (err != ESP_OK) {
          LOG_E(TAG, "SPI transmit failed: %d", err);
          return LT_L1_SPI_ERROR;
      }
      return LT_OK;
  }
  ```

- File: `components/cdc_hal/src/libtropic_port_esp32.cpp:74-88` - SPI bus acquisition uses `portMAX_DELAY`:
  ```cpp
  extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
      // ...
      if (device->spi) {
          esp_err_t err = spi_device_acquire_bus(device->spi, portMAX_DELAY);  // Infinite wait!
          if (err != ESP_OK) {
              LOG_E(TAG, "Failed to acquire SPI bus: %d", err);
              return LT_L1_SPI_ERROR;
          }
      }
      // ...
  }
  ```

## Impact
- **Deadlock risk**: If another task holds the SPI bus indefinitely, TROPIC01 operations can hang forever
- **No timeout propagation**: Upper layers cannot set operation-specific timeouts
- **System responsiveness**: Long-blocking SPI transfers can stall the entire system

## Recommended Fix
Implement timeout-aware SPI operations:

1. Replace `spi_device_polling_transmit` with `spi_device_transmit` and use `spi_device_acquire_bus` with timeout:
   ```cpp
   extern "C" lt_ret_t lt_port_spi_transfer(lt_l2_state_t *s2, uint8_t offset, uint16_t tx_len,
                                            uint32_t timeout_ms) {
      if (!s2 || !s2->device) {
          return LT_PARAM_ERR;
      }
      lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
      if (!device->spi) {
          LOG_E(TAG, "SPI not initialized");
          return LT_L1_SPI_ERROR;
      }

      // Acquire bus with timeout
      esp_err_t err = spi_device_acquire_bus(device->spi, pdMS_TO_TICKS(timeout_ms));
      if (err != ESP_OK) {
          LOG_E(TAG, "SPI bus acquire timeout: %d", err);
          return LT_L1_SPI_ERROR;
      }

      // In-place SPI transfer (TX/RX share buffer)
      spi_transaction_t t = {};
      t.length = static_cast<size_t>(tx_len) * 8;
      t.tx_buffer = s2->buff + offset;
      t.rx_buffer = s2->buff + offset;
      err = spi_device_transmit(device->spi, &t);  // Use non-blocking variant

      spi_device_release_bus(device->spi);

      if (err != ESP_OK) {
          LOG_E(TAG, "SPI transmit failed: %d", err);
          return LT_L1_SPI_ERROR;
      }
      return LT_OK;
  }
  ```

2. For `lt_port_spi_csn_low`, use bounded timeout:
   ```cpp
   esp_err_t err = spi_device_acquire_bus(device->spi, pdMS_TO_TICKS(100));  // 100ms timeout
   ```

## References
- ESP-IDF SPI driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html
- TROPIC01 timing requirements: https://www.tropic-solutions.com/product/tropic01/
