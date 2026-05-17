---
title: "[MEDIUM] Blocking SPI polling transfers in E-Paper display driver"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "spi"
  - "display"
---

## Summary

The E-Paper display driver (`components/CalEPD/epdspi.cpp` and related files) uses `spi_device_polling_transmit()` for all SPI transfers. This function blocks the calling task until the transfer is complete, which can take several milliseconds for large data transfers (e.g., full screen updates).

**Affected files:**
- `components/CalEPD/epdspi.cpp` (lines 96, 122, 158, 186, 206, 224, 244, 262, 284, 339, 352, 365, 378)
- `components/CalEPD/epd4spi.cpp` (27 occurrences of `spi_device_polling_transmit`)
- `components/CalEPD/epdspi2cs.cpp` (10 occurrences)
- `components/CalEPD/models/plasticlogic/epdspi2cs.cpp` (4 occurrences)

**Evidence:**
```cpp
// components/CalEPD/epdspi.cpp:158
ret=spi_device_polling_transmit(spi, &t);  //Transmit!
assert(ret==ESP_OK);
```

The comment in the code acknowledges this:
```cpp
/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
```

## Impact

1. **UI freeze during display updates**: When updating the E-Paper display (especially full refreshes), the main UI thread blocks for 100-500ms depending on the display size and update type.

2. **Missed input events**: During blocking SPI transfers, keypad input events may be delayed or buffered, causing perceived lag.

3. **No concurrent operations**: While waiting for SPI, the system cannot process other events (USB, Bluetooth, etc.).

4. **E-Paper specific**: E-Paper displays are already slow (200-500ms for full refresh), but blocking SPI adds additional latency on top of the panel's inherent slowness.

## Evidence

**File: `components/CalEPD/epdspi.cpp`**
- Line 96: `ret=spi_device_polling_transmit(spi, &t);` (cmd function)
- Line 122: `ret=spi_device_polling_transmit(spi, &t);` (data single byte)
- Line 158: `ret=spi_device_polling_transmit(spi, &t);` (data buffer)

**File: `components/CalEPD/epd4spi.cpp`**
- Lines 130-378: 27 occurrences of `spi_device_polling_transmit`

**File: `components/CalEPD/models/wave12i48.cpp`**
- Line 204: `vTaskDelay(1);` inside loops (additional blocking delays)
- Line 211: `vTaskDelay(pdMS_TO_TICKS(200));`

Total blocking SPI calls across all display drivers: **~50+ occurrences**

## Recommended Fix

1. **Use non-blocking SPI with transactions queue**:
   ```cpp
   // Replace polling with transaction queue
   spi_transaction_t t = {};
   t.length = len * 8;
   t.tx_buffer = data;
   
   // Use polling_transmit only for small commands (< 10 bytes)
   if (len <= 10) {
       spi_device_polling_transmit(spi, &t);
   } else {
       // Use interrupt-driven transmit for larger data
       spi_device_transmit(spi, &t);  // Non-blocking
   }
   ```

2. **Offload display updates to a dedicated task**:
   - Create a `display_task` with lower priority than the main UI task
   - Use a queue to send display commands/data
   - Main UI can continue processing while display updates happen in background

3. **Batch display operations**:
   - Collect multiple display updates and send them in fewer, larger transactions
   - Reduce SPI overhead by minimizing transaction setup time

4. **Add yield points during long operations**:
   ```cpp
   for (int i = 0; i < num_lines; i++) {
       send_line(data + i * line_len);
       if (i % 10 == 0) {
           vTaskDelay(pdMS_TO_TICKS(1));  // Allow other tasks to run
       }
   }
   ```

**Estimated effort**: 1-2 hours for initial implementation (offloading to dedicated task)

## References

- [ESP32 SPI Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/peripherals/spi_master.html)
- [spi_device_transmit vs spi_device_polling_transmit](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/peripherals/spi_master.html#transmitting-data)
- E-Paper displays typically require 200-500ms for full refresh; blocking SPI adds ~10-20ms overhead per transaction
