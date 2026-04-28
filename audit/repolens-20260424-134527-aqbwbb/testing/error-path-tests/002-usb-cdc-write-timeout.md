---
title: "[HIGH] USB CDC write loop has no timeout limit when buffer is full"
severity: HIGH
domain: error-path-tests
lens: usb-communication
labels:
  - "usb-cdc"
  - "timeout"
  - "blocking"
---

## Summary
In `components/usb_badge/usb_cdc.cpp` (lines 188-208), the `usb_cdc_write()` function blocks indefinitely when the USB CDC transmit buffer is full. The function uses a `while` loop with `tud_cdc_write_available()==0` check but has no timeout limit, potentially causing the calling task to hang forever if the USB host doesn't consume data.

**Files:**
- `components/usb_badge/usb_cdc.cpp:188-208`

## Impact
1. **System Hang**: Any task calling `usb_cdc_write()` can be blocked indefinitely
2. **Deadlock Risk**: If the task holding a mutex waits for USB to send a status update, deadlock occurs
3. **Watchdog Trigger**: Long blocking may trigger ESP32 watchdog timers
4. **No Recovery**: Once stuck, the only recovery is a hard reset
5. **Real-world Scenario**: USB host disconnects or buffer fills (e.g., slow terminal), system freezes

## Evidence
From `components/usb_badge/usb_cdc.cpp`:

```cpp
// Lines 188-208: usb_cdc_write() - no timeout limit
size_t usb_cdc_write(const uint8_t* data, size_t len) {
    if (!usb_cdc_available()) {
        return 0;
    }

    size_t totalSent = 0;
    while (totalSent < len) {
        // Get available space in TX buffer
        uint16_t available = tud_cdc_write_available();
        
        // BLOCKS INDEFINITELY if buffer stays full!
        while (available == 0) {
            // No timeout, no max iterations
            available = tud_cdc_write_available();
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        // Calculate how much to send
        uint16_t toSend = min(available, len - totalSent);
        
        // Send data
        uint16_t sent = tud_cdc_write(data + totalSent, toSend);
        totalSent += sent;
        
        // Tiny delay
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return totalSent;
}
```

**Problem:**
- Inner `while (available == 0)` loop has **no counter, no timeout**
- If USB host doesn't read data, this loops forever
- `vTaskDelay(1ms)` prevents watchdog but doesn't solve blocking
- No error return to caller

## Recommended Fix
Add a timeout limit to the write loop:

1. **Define a write timeout constant**:
   ```cpp
   #define CDC_WRITE_TIMEOUT_MS 500  // 500ms max wait for buffer space
   ```

2. **Modify usb_cdc_write() with timeout**:
   ```cpp
   size_t usb_cdc_write(const uint8_t* data, size_t len) {
       if (!usb_cdc_available()) {
           return 0;
       }

       size_t totalSent = 0;
       uint32_t startTime = esp_timer_get_time();
       
       while (totalSent < len) {
           uint16_t available = tud_cdc_write_available();
           
           // Wait for buffer space with timeout
           while (available == 0) {
               vTaskDelay(pdMS_TO_TICKS(1));
               available = tud_cdc_write_available();
               
               // Check timeout
               uint32_t elapsed = (esp_timer_get_time() - startTime) / 1000;
               if (elapsed > CDC_WRITE_TIMEOUT_MS) {
                   LOG_W("CDC", "Write timeout after %lu ms, sent %zu/%zu bytes", 
                         elapsed, totalSent, len);
                   return totalSent;  // Return what we sent so far
               }
           }

           uint16_t toSend = min(available, len - totalSent);
           uint16_t sent = tud_cdc_write(data + totalSent, toSend);
           totalSent += sent;
           vTaskDelay(pdMS_TO_TICKS(1));
       }

       return totalSent;
   }
   ```

3. **Update callers to handle partial writes**:
   ```cpp
   // Example: In FIDO2 module
   size_t sent = usb_cdc_write(buffer, len);
   if (sent < len) {
       LOG_W("FIDO2", "Partial write: %zu/%zu bytes sent", sent, len);
       // Retry logic or queue for later
   }
   ```

4. **Add test cases** for timeout scenario:
   - Mock `tud_cdc_write_available()` to return 0 for extended period
   - Verify function returns partial count after timeout
   - Verify no task hangs longer than timeout

## References
- USB CDC Class Specification: https://usb.org/document-library/cdc-12
- ESP32 Task Watchdog Timer: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/watchdog.html
- Blocking I/O Best Practices: https://www.embedded.com/design/prototyping-and-development/4026321/Non-blocking-I-O-in-embedded-systems
- TinyUSB Documentation: https://github.com/hathach/tinyusb/tree/master/docs

</content>