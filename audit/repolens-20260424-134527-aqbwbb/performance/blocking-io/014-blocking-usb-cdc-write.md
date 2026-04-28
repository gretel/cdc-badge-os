---
title: "[MEDIUM] Blocking USB CDC write loop"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "usb"
  - "cdc"
  - "serial"
---

## Summary

The USB CDC implementation in `components/usb_badge/usb_cdc.cpp` uses a blocking write loop that waits indefinitely for USB buffer space, potentially stalling the calling task for extended periods.

**Affected file:**
- `components/usb_badge/usb_cdc.cpp` (lines ~100-120, in `cdc_write()` function)

**Evidence:**
```cpp
// components/usb_badge/usb_cdc.cpp:cdc_write()
size_t cdc_write(const uint8_t* data, size_t len) {
    if (!g_usb_started) return 0;

    size_t written = 0;
    while (written < len) {
        size_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_cdc_write_flush();
            vTaskDelay(1);  // Busy-wait loop!
            continue;
        }

        size_t chunk = (len - written > avail) ? avail : (len - written);
        size_t sent = tud_cdc_write(data + written, chunk);
        written += sent;

        if (sent == 0) break;
    }
    return written;
}
```

**Blocking pattern:**
```
Task (e.g., logging): cdc_write(buffer, 1024);
                     -> tud_cdc_write_available() = 0
                     -> tud_cdc_write_flush()
                     -> vTaskDelay(1)  // Sleep 1ms
                     -> loop back
                     -> repeats until USB buffer has space
                     -> Could take 100-500ms depending on USB host
```

## Impact

1. **Task blocking**: The calling task blocks for the entire duration of the write operation.

2. **Variable latency**: USB write latency depends on host polling rate (1ms typical, up to 10ms for low-speed).

3. **Large data stalls**: Writing large buffers (e.g., QR data, logs) can block for 100-500ms.

4. **No timeout**: The loop continues indefinitely until all data is written.

5. **Busy-wait overhead**: `vTaskDelay(1)` wakes the task every 1ms, consuming CPU.

## Evidence

**USB CDC characteristics:**
- TinyUSB CDC buffer: ~256-512 bytes (configurable)
- USB polling rate: 1ms (full-speed), 10ms (low-speed)
- Typical throughput: ~10-50 KB/s

**Blocking scenarios:**
- Small writes (< 64 bytes): ~1-5ms
- Medium writes (256 bytes): ~10-50ms
- Large writes (1024 bytes): ~50-200ms

**Called from:**
- `components/cdc_log/src/cdc_log.cpp`: Logging (hot path!)
- Serial command handlers
- FIDO2 debug output

**Example blocking:**
```cpp
// Log a large message
LOG_I(TAG, "FIDO2 assertion: %s", large_json_string);  // ~500 bytes
// Blocks for ~50-100ms while USB writes
```

## Recommended Fix

1. **Add timeout to write loop** (30 min):
```cpp
size_t cdc_write(const uint8_t* data, size_t len) {
    if (!g_usb_started) return 0;

    size_t written = 0;
    uint32_t startMs = xTaskGetTickCount() * portTICK_PERIOD_MS;
    const uint32_t timeoutMs = 100;  // 100ms timeout

    while (written < len) {
        size_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_cdc_write_flush();
            vTaskDelay(1);

            // Check timeout
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS - startMs) > timeoutMs) {
                LOG_W(TAG, "CDC write timeout, %d bytes remaining", len - written);
                break;  // Return what we have
            }
            continue;
        }

        size_t chunk = (len - written > avail) ? avail : (len - written);
        size_t sent = tud_cdc_write(data + written, chunk);
        written += sent;

        if (sent == 0) break;
    }
    return written;
}
```

2. **Use non-blocking write with queue** (1 hour):
```cpp
// Add write queue for async writes
static QueueHandle_t s_writeQueue;
static const size_t WRITE_QUEUE_SIZE = 16;

bool cdc_write_async(const uint8_t* data, size_t len) {
    if (!g_usb_started) return false;

    // Copy to queue (non-blocking)
    uint8_t* buf = malloc(len);
    memcpy(buf, data, len);
    return xQueueSend(s_writeQueue, &buf, 0) == pdTRUE;
}

// Worker task
static void cdc_write_task(void* param) {
    while (true) {
        uint8_t* buf;
        if (xQueueReceive(s_writeQueue, &buf, pdMS_TO_TICKS(100))) {
            size_t len = strlen((char*)buf);
            cdc_write_blocking(buf, len);  // Original blocking write
            free(buf);
        }
    }
}
```

3. **Use TinyUSB write callback** (1 hour):
```cpp
// Implement tud_cdc_write_complete callback
void tud_cdc_write_complete(void) {
    // Notify waiting task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_writeSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Non-blocking write with callback
bool cdc_write_nb(const uint8_t* data, size_t len) {
    size_t avail = tud_cdc_write_available();
    if (avail == 0) return false;  // Try later

    size_t chunk = (len > avail) ? avail : len;
    size_t sent = tud_cdc_write(data, chunk);
    return sent > 0;
}
```

4. **Increase USB buffer size** (15 min):
```cpp
// In tusb_config.h
#define CFG_TUD_CDC_RX_BUFSIZE  512  // Default: 256
#define CFG_TUD_CDC_TX_BUFSIZE  512  // Default: 256
```

5. **Batch log writes** (30 min):
```cpp
// Buffer logs in RAM, flush periodically
static char logBuffer[1024];
static size_t logPos = 0;

void cdc_log_flush() {
    if (logPos > 0) {
        cdc_write((uint8_t*)logBuffer, logPos);
        logPos = 0;
    }
}

// Call flush every 100ms or when buffer full
```

**Estimated effort**: 30 min - 1 hour for timeout implementation

## References

- [TinyUSB CDC class](https://github.com/hathach/tinyusb/tree/master/src/class/cdc)
- [ESP-IDF USB CDC](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_cdc.html)
- USB CDC throughput: ~10-50 KB/s typical
- [TinyUSB configuration](https://github.com/hathach/tinyusb/blob/master/src/tusb_config.h)

</content>