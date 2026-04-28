---
title: "[MEDIUM] USB CDC task created but never cleaned up on shutdown"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/usb_badge/usb_cdc.cpp`, the USB CDC task is created in `start()` but never joined or cleaned up in a corresponding `stop()` method. The task handle `s_usbTask` is stored but only used to get the task handle, not to properly terminate the task. This means when the system shuts down or the USB stack needs to be reinitialized, the USB task continues running potentially accessing freed resources.

**Location:** `components/usb_badge/usb_cdc.cpp:95-112`

## Impact

1. **Resource leak**: The USB task continues running after `start()` is called, consuming stack space and CPU time even if the USB stack is logically "stopped".

2. **Use-after-free risk**: If the USB stack is deinitialized and reinitialized (e.g., during re-enumeration), the old task might still be running and accessing freed memory.

3. **Double-initialization risk**: If `start()` is called multiple times, multiple tasks could be created since there's no check to prevent this (the `g_usb_started` flag only prevents re-initialization of the stack, not task creation).

4. **Shutdown ordering issues**: During system shutdown, other resources might be freed before the USB task is done using them, leading to crashes.

## Evidence

**File: `components/usb_badge/usb_cdc.cpp`**

1. **Task created but never joined (line 95-112):**
```cpp
static TaskHandle_t s_usbTask = nullptr;

bool usb_cdc_start(void) {
    if (g_usb_started) {
        return true;
    }

    s_usbTask = xTaskCreate(usbTask, "usb_cdc", 2048, NULL, 5, NULL);  // Created
    if (s_usbTask == NULL) {
        return false;
    }

    g_usb_started = true;
    return true;
}
```

2. **No stop() function exists**: There is no corresponding `usb_cdc_stop()` function to clean up the task.

3. **Task handle stored but unused for cleanup (line 33-37):**
```cpp
static bool g_usb_started = false;
static uint8_t s_send_buf[CONFIG_TINYUSB_CDC_ENABLED];
static TaskHandle_t s_usbTask = nullptr;  // Stored but never used to terminate
```

4. **usbTask function runs indefinitely (line 71-91):**
```cpp
static void usbTask(void* arg) {
    while (true) {
        // Check if USB is connected
        if (tud_cdc_connected()) {
            // Process data...
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Runs forever
    }
}
```

5. **TinyUSB init called but no deinit (line 47-63):**
```cpp
// In usbTask:
static bool first = true;
if (first) {
    // PHY init...
    tusb_init();  // Initialized
    first = false;
}
// ... but tusb_deinit() never called anywhere
```

## Recommended Fix

Add a `usb_cdc_stop()` function that properly cleans up the USB task and stack:

1. **Add stop function:**
```cpp
bool usb_cdc_stop(void) {
    if (!g_usb_started) {
        return true;
    }

    // Signal task to stop (add a flag)
    g_usb_started = false;

    // Wait for task to exit (with timeout)
    if (s_usbTask) {
        // Option 1: Suspend and delete
        vTaskSuspend(s_usbTask);
        vTaskDelete(s_usbTask);
        s_usbTask = NULL;

        // Option 2: Let task exit naturally (requires task to check flag)
        // vTaskDelay(pdMS_TO_TICKS(100));  // Wait for task to check flag
    }

    // Deinitialize TinyUSB
    tusb_deinit();

    return true;
}
```

2. **Modify task to check exit flag:**
```cpp
static void usbTask(void* arg) {
    while (g_usb_started) {  // Check flag in loop condition
        // Check if USB is connected
        if (tud_cdc_connected()) {
            // Process data...
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Exit naturally when g_usb_started is false
}
```

3. **Add to header:**
```cpp
// usb_cdc.h
bool usb_cdc_start(void);
bool usb_cdc_stop(void);  // Add this
```

4. **Call in system shutdown:**
```cpp
// In main.cpp or appropriate shutdown sequence
usb_cdc_stop();
```

## References

- [FreeRTOS Task API](https://www.freertos.org/tasks.html) - Task creation and deletion
- [TinyUSB Porting Guide](https://github.com/hathach/tinyusb/tree/master/examples) - Proper init/deinit
- [ESP-IDF USB](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/usb_periph.html) - USB peripheral lifecycle

</content>