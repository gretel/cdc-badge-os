---
title: "[LOW] USB CDC write uses spin-loop without maximum iteration limit"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "spin-loop"
  - "usb"
  - "flow-control"
---

## Summary
In `components/usb_badge/usb_cdc.cpp:183-204`, the `usb_cdc_write()` function uses a spin-loop to wait for USB CDC endpoint space. While it includes delays, there's no maximum iteration count or timeout, which could cause the function to hang indefinitely if the USB host is slow to read data or if the endpoint is stalled.

**Location:** `components/usb_badge/usb_cdc.cpp:183-204`

```cpp
size_t usb_cdc_write(const uint8_t* data, size_t len) {
    if (!g_usb_started || !data || len == 0) return 0;

    size_t written = 0;
    while (written < len) {
        size_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_cdc_write_flush();
            vTaskDelay(1);  // Spin with 1ms delay
            continue;       // No max iterations!
        }

        size_t chunk = (len - written > avail) ? avail : (len - written);
        size_t sent = tud_cdc_write(data + written, chunk);
        written += sent;

        if (sent == 0) break;  // Exit if nothing sent
    }

    tud_cdc_write_flush();
    return written;
}
```

## Impact
- **Potential hang:** If USB endpoint stalls or host stops reading, the function could block forever
- **No timeout:** Callers have no way to specify a timeout or get feedback about slow USB
- **Blocking behavior:** The function blocks the calling task until all data is sent, which could be slow for large buffers

## Evidence
File: `components/usb_badge/usb_cdc.cpp:183-204`
- Line 185: `while (written < len)` - no iteration limit
- Line 189: `vTaskDelay(1)` - only 1ms delay, could spin for a very long time
- Line 194: `if (sent == 0) break` - only exits if `tud_cdc_write()` returns 0, but doesn't check if endpoint is stalled
- No timeout parameter or maximum wait constant

## Recommended Fix
Add timeout and iteration limit:

```cpp
// Add timeout constant
static constexpr uint32_t USB_CDC_WRITE_TIMEOUT_MS = 500;  // 500ms max

/**
 * \brief Writes byte buffer to USB CDC endpoint with timeout.
 * \param data Data buffer.
 * \param len Number of bytes to write.
 * \param timeoutMs Maximum time to wait in milliseconds (0 = no timeout).
 * \return Number of bytes written.
 */
size_t usb_cdc_write_with_timeout(const uint8_t* data, size_t len, uint32_t timeoutMs) {
    if (!g_usb_started || !data || len == 0) return 0;

    size_t written = 0;
    uint32_t startTime = esp_timer_get_time() / 1000;
    uint8_t stallCount = 0;
    static constexpr uint8_t MAX_STALL_COUNT = 10;  // 10 consecutive stalls

    while (written < len) {
        // Check timeout
        if (timeoutMs > 0) {
            uint32_t elapsed = esp_timer_get_time() / 1000 - startTime;
            if (elapsed >= timeoutMs) {
                LOG_W(TAG, "USB write timeout after %lu ms (wrote %zu/%zu bytes)",
                      elapsed, written, len);
                break;
            }
        }

        size_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_cdc_write_flush();
            vTaskDelay(1);
            
            // Track stall count
            stallCount++;
            if (stallCount >= MAX_STALL_COUNT) {
                LOG_W(TAG, "USB endpoint stalled after %d attempts", stallCount);
                break;
            }
            continue;
        }
        
        stallCount = 0;  // Reset on success

        size_t chunk = (len - written > avail) ? avail : (len - written);
        size_t sent = tud_cdc_write(data + written, chunk);
        written += sent;

        if (sent == 0) {
            stallCount++;
            if (stallCount >= MAX_STALL_COUNT) {
                LOG_W(TAG, "USB write stalled after %d attempts", stallCount);
                break;
            }
        }
    }

    tud_cdc_write_flush();
    return written;
}

/**
 * \brief Writes byte buffer to USB CDC endpoint (500ms timeout).
 * \param data Data buffer.
 * \param len Number of bytes to write.
 * \return Number of bytes written.
 */
size_t usb_cdc_write(const uint8_t* data, size_t len) {
    return usb_cdc_write_with_timeout(data, len, USB_CDC_WRITE_TIMEOUT_MS);
}

// Optional: Non-blocking version
size_t usb_cdc_write_nonblocking(const uint8_t* data, size_t len) {
    return usb_cdc_write_with_timeout(data, len, 0);  // 0 = no timeout
}
```

**Alternative: Add iteration-based limit**
```cpp
// Simpler approach: limit iterations
static constexpr uint16_t MAX_WRITE_ITERATIONS = 500;  // ~500ms at 1ms delay

size_t usb_cdc_write(const uint8_t* data, size_t len) {
    if (!g_usb_started || !data || len == 0) return 0;

    size_t written = 0;
    uint16_t iterations = 0;
    
    while (written < len && iterations < MAX_WRITE_ITERATIONS) {
        size_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_cdc_write_flush();
            vTaskDelay(1);
            iterations++;
            continue;
        }

        size_t chunk = (len - written > avail) ? avail : (len - written);
        size_t sent = tud_cdc_write(data + written, chunk);
        written += sent;

        if (sent == 0) {
            iterations++;
        }
    }

    if (iterations >= MAX_WRITE_ITERATIONS) {
        LOG_W(TAG, "USB write hit iteration limit (wrote %zu/%zu bytes)", written, len);
    }

    tud_cdc_write_flush();
    return written;
}
```

## References
- [TinyUSB CDC Class](https://github.com/hathach/tinyusb/tree/master/src/class/cdc)
- [USB CDC Flow Control](https://www.usb.org/document-library/cdc-v120-errata)
- ESP-IDF USB OTG: `components/esp_usb/`
